#include "mediamanager.h"

#include "filtergraph.h"
#include "rtpstreamer.h"
#include "filter.h"
#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "gui/videoviewfactory.h"
#include "sip/sdptypes.h"

#include <QHostAddress>
#include <QtEndian>
#include <QDebug>

MediaManager::MediaManager():
  stats_(nullptr),
  fg_(new FilterGraph()),
  session_(nullptr),
  streamer_(new RTPStreamer()),
  mic_(true),
  camera_(true)
{}

MediaManager::~MediaManager()
{
  fg_->running(false);
  fg_->uninit();
}

void MediaManager::init(std::shared_ptr<VideoviewFactory> viewfactory, StatisticsInterface *stats)
{
  qDebug() << "Iniating media manager";
  viewfactory_ = viewfactory;
  streamer_->init(stats);
  streamer_->start();
  fg_->init(viewfactory_->getVideo(0), stats); // 0 is the selfview index. The view should be created by GUI
}

void MediaManager::uninit()
{
  qDebug() << "Destroying media manager";
  // first filter graph, then streamer because of the rtpfilters
  fg_->running(false);
  fg_->uninit();

  streamer_->stop();
  streamer_->uninit();
}

void MediaManager::updateSettings()
{
  fg_->updateSettings();
  fg_->camera(camera_); // kind of a hack to make sure the camera/mic state is preserved
  fg_->mic(mic_);
}

void MediaManager::createOutgoingMedia(uint32_t sessionID, const std::shared_ptr<SDPMessageInfo> peerInfo)
{
  uint16_t sendAudioPort = 0;
  uint16_t sendVideoPort = 0;

  for(auto media : peerInfo->media)
  {
    if(media.type == "audio" && sendAudioPort == 0)
    {
      sendAudioPort = media.receivePort;
    }
    else if(media.type == "video" && sendVideoPort == 0)
    {
      sendVideoPort = media.receivePort;
    }
  }

  if(sendAudioPort == 0 || sendVideoPort == 0)
  {
    qWarning() << "ERROR: Faulty peerInfo at mediamanager createOutgoingStreams";
    return;
  }

  std::shared_ptr<Filter> audioFramedSource = streamer_->addSendAudio(sessionID, sendAudioPort);
  fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(audioFramedSource));

  std::shared_ptr<Filter> videoFramedSource = streamer_->addSendVideo(sessionID, sendVideoPort);
  fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(videoFramedSource));

}

void MediaManager::createIncomingMedia(uint32_t sessionID, const std::shared_ptr<SDPMessageInfo> localInfo)
{
  uint16_t recvAudioPort = 0;
  uint16_t recvVideoPort = 0;

  for(auto media : localInfo->media)
  {
    if(media.type == "audio" && recvAudioPort == 0)
    {
      recvAudioPort = media.receivePort;
    }
    else if(media.type == "video" && recvVideoPort == 0)
    {
      recvVideoPort = media.receivePort;
    }
  }

  if(recvAudioPort == 0 || recvVideoPort == 0)
  {
    qWarning() << "ERROR: Faulty peerInfo at mediamanager createOutgoingStreams";
    return;
  }


  std::shared_ptr<Filter> videoSink = streamer_->addReceiveVideo(sessionID, recvVideoPort);
  std::shared_ptr<Filter> audioSink = streamer_->addReceiveAudio(sessionID, recvAudioPort);

  fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(videoSink), viewfactory_->getVideo(sessionID));
  fg_->receiveAudioFrom(sessionID, std::shared_ptr<Filter>(audioSink));
}

void MediaManager::addParticipant(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                    const std::shared_ptr<SDPMessageInfo> localInfo)
{
  QHostAddress address;
  address.setAddress(peerInfo->connection_address);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  // Open necessary ports and create filters for sending and receiving
  if(!streamer_->addPeer(ip, sessionID))
  {
    qCritical() << "Error creating RTP peer. Simultaneous destruction?";
    return;
  }

  createOutgoingMedia(sessionID, peerInfo);
  createIncomingMedia(sessionID, localInfo);

  fg_->print();
}


void MediaManager::removeParticipant(uint32_t sessionID)
{
  fg_->removeParticipant(sessionID);
  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
  streamer_->removePeer(sessionID);
  qDebug() << "Session " << sessionID << " media removed.";
}

void MediaManager::endAllCalls()
{
  fg_->removeAllParticipants();
  streamer_->removeAllPeers();

  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
}

bool MediaManager::toggleMic()
{
  mic_ = !mic_;
  fg_->mic(mic_);
  return mic_;
}

bool MediaManager::toggleCamera()
{
  camera_ = !camera_;
  fg_->camera(camera_);
  return camera_;
}
