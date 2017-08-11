#include "mediamanager.h"

#include "filtergraph.h"
#include "rtpstreamer.h"
#include "filter.h"
#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"

#include <QDebug>

MediaManager::MediaManager(StatisticsInterface *stats):
  stats_(stats),
  fg_(new FilterGraph(stats)),
  session_(NULL),
  streamer_(new RTPStreamer(stats)),
  mic_(true),
  camera_(true)
{}

MediaManager::~MediaManager()
{
  fg_->running(false);
  fg_->uninit();
}

void MediaManager::init(VideoWidget *selfView)
{
  qDebug() << "Iniating media manager";
  streamer_->init();
  streamer_->start();
  fg_->init(selfView);
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
}

void MediaManager::addParticipant(QString callID, in_addr ip, uint16_t sendAudioPort, uint16_t recvAudioPort,
                                 uint16_t sendVideoPort, uint16_t recvVideoPort, VideoWidget *view)
{
  qDebug() << "Adding participant";

  // Open necessary ports and create filters for sending and receiving
  PeerID streamID = streamer_->addPeer(ip);

  if(ids_.find(callID) != ids_.end())
  {
    removeParticipant(callID);
  }
  ids_[callID] = streamID;

  if(streamID == -1)
  {
    qCritical() << "Error creating RTP peer";
    return;
  }

  qDebug() << "Creating connections";

  std::shared_ptr<Filter> videoFramedSource = streamer_->addSendVideo(streamID, sendVideoPort);
  std::shared_ptr<Filter> videoSink =streamer_->addReceiveVideo(streamID, recvVideoPort);
  std::shared_ptr<Filter> audioFramedSource = streamer_->addSendAudio(streamID, sendAudioPort);
  std::shared_ptr<Filter> audioSink = streamer_->addReceiveAudio(streamID, recvAudioPort);

  qDebug() << "Modifying filter graph";

  // create filter graphs for this participant
  fg_->sendVideoto(streamID, std::shared_ptr<Filter>(videoFramedSource));
  fg_->receiveVideoFrom(streamID, std::shared_ptr<Filter>(videoSink), view);
  fg_->sendAudioTo(streamID, std::shared_ptr<Filter>(audioFramedSource));
  fg_->receiveAudioFrom(streamID, std::shared_ptr<Filter>(audioSink));

  qDebug() << "Participant added";

  fg_->print();
}

void MediaManager::removeParticipant(QString callID)
{
  if(ids_.find(callID) == ids_.end())
  {
    qWarning() << "WARNING: No callID found in mediamanager:" << callID;
    return;
  }
  fg_->removeParticipant(ids_[callID]);
  streamer_->removePeer(ids_[callID]);
  ids_.erase(callID);
  qDebug() << "Participant " << callID << "removed.";
}

void MediaManager::endAllCalls()
{
  qDebug() << "Destroying" << ids_.size() << "calls.";
  for(auto caller : ids_)
  {
    fg_->removeParticipant(caller.second);
    streamer_->removePeer(caller.second);
  }
  ids_.clear();
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
