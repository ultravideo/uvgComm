#include "callmanager.h"

#include <QDebug>


CallManager::CallManager(StatisticsInterface *stats):
  stats_(stats),
  fg_(new FilterGraph(stats)),
  call_neg_(new CallNegotiation),
  session_(NULL),
  streamer_(new RTPStreamer(stats)),
  mic_(true),
  camera_(true)
{}

CallManager::~CallManager()
{
  fg_->uninit();
}

void CallManager::init()
{
  if(call_neg_)
  {
    call_neg_->init();
  }
}

void CallManager::uninit()
{
  // first filter graph, then streamer because of the rtpfilters
  fg_->running(false);
  fg_->uninit();

  streamer_->stop();
}

// registers a contact for activity monitoring
void CallManager::registerContact(in_addr ip)
{
  Q_UNUSED(ip)
}

void CallManager::startCall(VideoWidget *selfView, QSize resolution)
{
  streamer_->start();
  fg_->init(selfView, resolution);
}

void CallManager::addParticipant(in_addr ip, uint16_t port, VideoWidget *view)
{
  qDebug() << "Adding participant";

  // Open necessary ports and create filters for sending and receiving
  PeerID streamID = streamer_->addPeer(ip);

  if(streamID == -1)
  {
    qCritical() << "Error creating RTP peer";
    return;
  }

  qDebug() << "Creating connections";

  Filter *videoFramedSource = streamer_->addSendVideo(streamID, port);
  Filter *videoSink =streamer_->addReceiveVideo(streamID, port);
  Filter *audioFramedSource = streamer_->addSendAudio(streamID, port + 1000);
  Filter *audioSink = streamer_->addReceiveAudio(streamID, port + 1000);

  qDebug() << "Modifying filter graph";

  // create filter graphs for this participant
  fg_->sendVideoto(streamID, videoFramedSource);
  fg_->receiveVideoFrom(streamID, videoSink, view);
  fg_->sendAudioTo(streamID, audioFramedSource);
  fg_->receiveAudioFrom(streamID, audioSink);

  qDebug() << "Participant added";
}

void CallManager::kickParticipant()
{}

// callID in case more than one person is calling
void CallManager::joinCall(unsigned int callID)
{
  Q_UNUSED(callID)
}

void CallManager::endCall()
{}

void CallManager::streamToIP(in_addr ip, uint16_t port)
{
  Q_UNUSED(ip)
  Q_UNUSED(port)
}

void CallManager::receiveFromIP(in_addr ip, uint16_t port, VideoWidget* view)
{
  Q_UNUSED(ip)
  Q_UNUSED(port)
  Q_UNUSED(view)
}

bool CallManager::toggleMic()
{
  mic_ = !mic_;
  fg_->mic(mic_);
  return mic_;
}

bool CallManager::toggleCamera()
{
  camera_ = !camera_;
  fg_->camera(camera_);
  return camera_;
}
