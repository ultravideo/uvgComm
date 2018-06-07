#include "mediamanager.h"

#include "filtergraph.h"
#include "rtpstreamer.h"
#include "filter.h"
#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"

#include <QDebug>

MediaManager::MediaManager():
  stats_(NULL),
  fg_(new FilterGraph()),
  session_(NULL),
  streamer_(new RTPStreamer()),
  mic_(true),
  camera_(true)
{}

MediaManager::~MediaManager()
{
  fg_->running(false);
  fg_->uninit();
}

void MediaManager::init(VideoWidget *selfView, StatisticsInterface* stats)
{
  qDebug() << "Iniating media manager";
  streamer_->init(stats);
  streamer_->start();
  fg_->init(selfView, stats);
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

void MediaManager::addParticipant(uint32_t sessionID, in_addr ip, uint16_t sendAudioPort, uint16_t recvAudioPort,
                                 uint16_t sendVideoPort, uint16_t recvVideoPort, VideoWidget *view)
{
  // Open necessary ports and create filters for sending and receiving
  if(!streamer_->addPeer(ip, sessionID))
  {
    qCritical() << "Error creating RTP peer. Simultaneous destruction?";
    return;
  }

  qDebug() << "Creating connections for ID:" << sessionID;
  std::shared_ptr<Filter> videoFramedSource = streamer_->addSendVideo(sessionID, sendVideoPort);
  std::shared_ptr<Filter> videoSink =streamer_->addReceiveVideo(sessionID, recvVideoPort);
  std::shared_ptr<Filter> audioFramedSource = streamer_->addSendAudio(sessionID, sendAudioPort);
  std::shared_ptr<Filter> audioSink = streamer_->addReceiveAudio(sessionID, recvAudioPort);

  qDebug() << "Modifying filter graph for ID:" << sessionID;
  // create filter graphs for this participant
  fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(videoFramedSource));
  fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(videoSink), view);
  fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(audioFramedSource));
  fg_->receiveAudioFrom(sessionID, std::shared_ptr<Filter>(audioSink));

  qDebug() << " ================== Participant added with ID:" << sessionID << "===========================";

  // not working at the moment.
  //fg_->print();
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
