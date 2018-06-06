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

void MediaManager::addParticipant(uint32_t sessionID, in_addr ip, uint16_t sendAudioPort, uint16_t recvAudioPort,
                                 uint16_t sendVideoPort, uint16_t recvVideoPort, VideoWidget *view)
{
  Q_ASSERT(portsInUse_ + portsPerParticipant_ <= maxPortsOpen_);
  Q_ASSERT(portsReserved_ >= portsPerParticipant_);

  qDebug() << " ==================== Adding participant" << sessionID << "to media with ports left:" << maxPortsOpen_ - portsInUse_
           << "Reserved:" << portsReserved_ << "=====================";

  portsMutex_.lock();
  portsInUse_ += portsPerParticipant_;
  portsReserved_ -= portsPerParticipant_;
  portsMutex_.unlock();

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
  Q_ASSERT(portsInUse_ >= portsForSessionID(sessionID));

  fg_->removeParticipant(sessionID);
  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
  streamer_->removePeer(sessionID);
  qDebug() << "Session " << sessionID << " media removed.";

  qDebug() << "They have left the call. Ports in use:" << portsInUse_
           << "->" << portsInUse_ - portsForSessionID(sessionID);

  portsInUse_ -= portsForSessionID(sessionID);
}

void MediaManager::endAllCalls()
{
  fg_->removeAllParticipants();
  streamer_->removeAllPeers();

  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);

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
  qDebug() << "Could not fit more participants to call. Max ports:" << maxPortsOpen_
           << "Ports in use:" << portsInUse_ << "Ports reserved:" << portsReserved_;
  return false;
}

void MediaManager::freePorts()
{
  Q_ASSERT(portsReserved_ >= portsPerParticipant_);
  portsMutex_.lock();
  if(portsReserved_ >= portsPerParticipant_)
  {
    portsReserved_ -= portsPerParticipant_;
  }
  else
  {
    qDebug() << "WARNING: Trying to free ports that have not been reserved!";
  }

  portsMutex_.unlock();
}

