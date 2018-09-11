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

void MediaManager::addParticipant(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                    const std::shared_ptr<SDPMessageInfo> localInfo)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9

  if(peerInfo->startTime != 0 || localInfo->startTime != 0)
  {
    qWarning() << "ERROR: Non zero start-time not supported!";
  }

  if(peerInfo->stopTime != 0 || localInfo->stopTime != 0)
  {
    qWarning() << "ERROR: Non zero stop-time not supported!";
  }

  if(peerInfo->connection_nettype == "IN")
  {
    QHostAddress address;
    address.setAddress(peerInfo->connection_address);

    if(peerInfo->connection_addrtype == "IP4")
    {
      in_addr ip;
      ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

      // TODO: Make it possible to have a separate ip address for each mediastream by fixing this.
      if(!streamer_->addPeer(ip, sessionID))
      {
        qCritical() << "Error creating RTP peer. Simultaneous destruction?";
        return;
      }
    }
    else if(peerInfo->connection_addrtype == "IP6")
    {
      qDebug() << "ERROR: IPv6 not supported in media creation";
      return;
    }
  }
  else
  {
    qWarning() << "ERROR: What are we using if not the internet!?";
    return;
  }

  // create each agreed media stream
  for(auto media : peerInfo->media)
  {
    createOutgoingMedia(sessionID, media);
  }

  for(auto media : localInfo->media)
  {
    createIncomingMedia(sessionID, media);
  }

  fg_->print();
}


void MediaManager::createOutgoingMedia(uint32_t sessionID, const MediaInfo& media)
{
  std::shared_ptr<Filter> framedSource = streamer_->addSendStream(sessionID, media.receivePort,
                                                                  media.codecs.at(0).codec, media.rtpNums.at(0));
  if(media.type == "audio")
  {
    if(media.receivePort != 0 && !media.rtpNums.empty())
    {
      fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(framedSource));
    }
  }
  else if(media.type == "video")
  {
    if(media.receivePort != 0 && !media.rtpNums.empty())
    {
      fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(framedSource));
    }
    else
    {
      qWarning() << "ERROR: 0 as send videoport";
    }

  }
  else
  {
    qDebug() << "ERROR: Unsupported media type in :" << media.type;
  }
}

void MediaManager::createIncomingMedia(uint32_t sessionID, const MediaInfo &media)
{
  if(media.receivePort == 0)
  {
    qWarning() << "ERROR: Faulty peerInfo at mediamanager createOutgoingStreams";
    return;
  }
  std::shared_ptr<Filter> rtpSink = streamer_->addReceiveStream(sessionID, media.receivePort,
                                                                media.codecs.at(0).codec, media.rtpNums.at(0));

  if(media.type == "video")
  {
    fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(rtpSink), viewfactory_->getVideo(sessionID));
  }
  else if(media.type == "audio")
  {
    fg_->receiveAudioFrom(sessionID, std::shared_ptr<Filter>(rtpSink));
  }
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
