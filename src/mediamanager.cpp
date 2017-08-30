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
  camera_(true),
  portsPerParticipant_(4),
  maxPortsOpen_(42),
  portsInUse_(0),
  portsReserved_(0)
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

void MediaManager::addParticipant(QString callID, in_addr ip, uint16_t sendAudioPort, uint16_t recvAudioPort,
                                 uint16_t sendVideoPort, uint16_t recvVideoPort, VideoWidget *view)
{
  Q_ASSERT(portsInUse_ + portsPerParticipant_ <= maxPortsOpen_);
  Q_ASSERT(portsReserved_ >= portsPerParticipant_);

  qDebug() << "Adding participant to media with ports left:" << maxPortsOpen_ - portsInUse_
           << "Reserved:" << portsReserved_;

  portsMutex_.lock();
  portsInUse_ += portsPerParticipant_;
  portsReserved_ -= portsPerParticipant_;
  portsMutex_.unlock();

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

  qDebug() << "Creating connections for ID:" << streamID;
  std::shared_ptr<Filter> videoFramedSource = streamer_->addSendVideo(streamID, sendVideoPort);
  std::shared_ptr<Filter> videoSink =streamer_->addReceiveVideo(streamID, recvVideoPort);
  std::shared_ptr<Filter> audioFramedSource = streamer_->addSendAudio(streamID, sendAudioPort);
  std::shared_ptr<Filter> audioSink = streamer_->addReceiveAudio(streamID, recvAudioPort);

  qDebug() << "Modifying filter graph for ID:" << streamID;
  // create filter graphs for this participant
  fg_->sendVideoto(streamID, std::shared_ptr<Filter>(videoFramedSource));
  fg_->receiveVideoFrom(streamID, std::shared_ptr<Filter>(videoSink), view);
  fg_->sendAudioTo(streamID, std::shared_ptr<Filter>(audioFramedSource));
  fg_->receiveAudioFrom(streamID, std::shared_ptr<Filter>(audioSink));

  qDebug() << "Participant added with ID:" << streamID;

  fg_->print();
}

void MediaManager::removeParticipant(QString callID)
{
  Q_ASSERT(portsInUse_ >= portsForCallID(callID));

  if(ids_.find(callID) == ids_.end())
  {
    qWarning() << "WARNING: No callID found in mediamanager:" << callID;
    return;
  }
  fg_->removeParticipant(ids_[callID]);
  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
  streamer_->removePeer(ids_[callID]);
  ids_.erase(callID);
  qDebug() << "Participant " << callID << "removed.";

  qDebug() << "They have left the call. Ports in use:" << portsInUse_
           << "->" << portsInUse_ - portsForCallID(callID);

  portsInUse_ -= portsForCallID(callID);
}

void MediaManager::endAllCalls()
{
  qDebug() << "Destroying" << ids_.size() << "calls.";
  for(auto caller : ids_)
  {
    //cant use removeparticipant-function of media manager,  because iterator would break
    fg_->removeParticipant(caller.second);
    fg_->camera(camera_); // if the last participant was destroyed, restore camera state
    fg_->mic(mic_);
    streamer_->removePeer(caller.second);
  }
  ids_.clear();

  portsInUse_ = 0;
  portsReserved_ = 0;
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

bool MediaManager::reservePorts()
{
  portsMutex_.lock();
  if(portsReserved_ + portsPerParticipant_ + portsInUse_ <= maxPortsOpen_)
  {
    portsReserved_ += portsPerParticipant_;
    portsMutex_.unlock();
    return true;
  }
  portsMutex_.unlock();
  return false;
}

