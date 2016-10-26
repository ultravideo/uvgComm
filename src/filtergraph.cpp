#include "filtergraph.h"


#include "camerafilter.h"
#include "kvazaarfilter.h"
#include "rgb32toyuv.h"
#include "openhevcfilter.h"
#include "yuvtorgb32.h"
#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "displayfilter.h"

#include "audiocapturefilter.h"
#include "audiooutputdevice.h"
#include "audiooutput.h"
#include "opusencoderfilter.h"
#include "opusdecoderfilter.h"
#include "speexaecfilter.h"

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep
#endif
void qSleep(int ms)
{

#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
#endif
}




FilterGraph::FilterGraph(StatisticsInterface* stats):
  peers_(),
  videoSend_(),
  audioSend_(),
  selfView_(NULL),
  streamer_(stats),
  stats_(stats),
  format_()
{
  Q_ASSERT(stats);
  // TODO negotiate these values with all included filters
  format_.setSampleRate(48000);
  format_.setChannelCount(2);
  format_.setSampleSize(16);
  format_.setSampleType(QAudioFormat::SignedInt);
  format_.setByteOrder(QAudioFormat::LittleEndian);
  format_.setCodec("audio/pcm");
}

void FilterGraph::init(VideoWidget* selfView, QSize resolution)
{
  streamer_.start();

  resolution_ = resolution;
  selfView_ = selfView;
  frameRate_ = 30;

  initSelfView(selfView, resolution);
}

void FilterGraph::initSelfView(VideoWidget *selfView, QSize resolution)
{
  if(videoSend_.size() > 0)
  {
    destroyFilters(videoSend_);
  }

  // Sending video graph
  videoSend_.push_back(new CameraFilter(stats_, resolution));

  if(selfView)
  {
    // connect selfview to camera
    DisplayFilter* selfviewFilter = new DisplayFilter(stats_, selfView, 1111);
    selfviewFilter->setProperties(true);
    videoSend_.push_back(selfviewFilter);
    videoSend_.at(0)->addOutConnection(videoSend_.back());
    videoSend_.back()->start();
  }
}

void FilterGraph::initVideoSend(QSize resolution)
{
  if(videoSend_.size() != 2)
  {
    if(videoSend_.size() > 2)
    {
      destroyFilters(videoSend_);
    }
    initSelfView(selfView_, resolution_);
  }

  videoSend_.push_back(new RGB32toYUV(stats_));
  videoSend_.at(0)->addOutConnection(videoSend_.back()); // attach to camera
  videoSend_.back()->start();

  KvazaarFilter* kvz = new KvazaarFilter(stats_);
  kvz->init(resolution, frameRate_, 1, 0);
  videoSend_.push_back(kvz);
  videoSend_.at(videoSend_.size() - 2)->addOutConnection(videoSend_.back());
  videoSend_.back()->start();
}

void FilterGraph::initAudioSend()
{
  // Do this before adding participants, otherwise AEC filter wont get attached

  AudioCaptureFilter* capture = new AudioCaptureFilter(stats_);
  capture->initializeAudio(format_);
  audioSend_.push_back(capture);

  OpusEncoderFilter *encoder = new OpusEncoderFilter(stats_);
  encoder->init(format_);
  audioSend_.push_back(encoder);
  audioSend_.at(audioSend_.size() - 2)->addOutConnection(audioSend_.back());
  audioSend_.back()->start();
}

ParticipantID FilterGraph::addParticipant(in_addr ip, uint16_t port, VideoWidget* view,
                                          bool wantsAudio, bool sendsAudio,
                                          bool wantsVideo, bool sendsVideo)
{
  Q_ASSERT(stats_);

  if(!wantsAudio && !wantsVideo && !sendsAudio && !sendsVideo)
  {
    qWarning() << "WARNING: peer does not want or send any media";
    return -1;
  }

  // just in case it is wanted later. AEC filter has to be attached
  if(audioSend_.size() == 0 && wantsAudio)
  {
    initAudioSend();
  }

  if(wantsVideo && videoSend_.size() < 3)
  {
    initVideoSend(resolution_);
  }

  peers_.push_back(new Peer);
  peers_.back()->output = 0;
  peers_.back()->audioFramedSource = 0;
  peers_.back()->videoFramedSource = 0;
  peers_.back()->audioSink = 0;
  peers_.back()->videoSink = 0;
  peers_.back()->streamID = streamer_.addPeer(ip);
  if(peers_.back()->streamID == -1)
  {
    qCritical() << "Error creating RTP peer";
    return peers_.back()->streamID;
  }

  if(wantsAudio)
  {
    sendAudioTo(peers_.back(), port + 1000);
  }
  if(sendsAudio)
  {
    receiveAudioFrom(peers_.back(), port + 1000);
  }
  if(wantsVideo)
  {
    sendVideoto(peers_.back(), port);
  }
  if(sendsVideo && view != NULL)
  {
    receiveVideoFrom(peers_.back(), port, view);
  }
  else if(view == NULL)
  {
    qWarning() << "Warning: wanted to receive video, but no view available";
  }

  qDebug() << "Participant has been successfully added to call.";

  return peers_.back()->streamID;
}

void FilterGraph::sendVideoto(Peer* send, uint16_t port)
{
  Q_ASSERT(send && send->streamID != -1);

  streamer_.addSendVideo(send->streamID, port);

  Filter *framedSource = NULL;
  framedSource = streamer_.getSendFilter(send->streamID, HEVCVIDEO);

  send->videoFramedSource = framedSource;

  videoSend_.back()->addOutConnection(framedSource);
}

void FilterGraph::receiveVideoFrom(Peer* recv, uint16_t port, VideoWidget *view)
{
  Q_ASSERT(recv && recv->streamID != -1);

  streamer_.addReceiveVideo(recv->streamID, port);

  // Receiving video graph
  recv->videoSink = streamer_.getReceiveFilter(recv->streamID, HEVCVIDEO);
  //recv->videoReceive.push_back(rtpSink);

  OpenHEVCFilter* decoder =  new OpenHEVCFilter(stats_);
  decoder->init();
  recv->videoReceive.push_back(decoder);
  recv->videoSink->addOutConnection(recv->videoReceive.back());
  recv->videoReceive.back()->start();

  recv->videoReceive.push_back(new YUVtoRGB32(stats_));
  recv->videoReceive.at(recv->videoReceive.size()-2)->addOutConnection(recv->videoReceive.back());
  recv->videoReceive.back()->start();

  recv->videoReceive.push_back(new DisplayFilter(stats_, view, recv->streamID));
  recv->videoReceive.at(recv->videoReceive.size()-2)->addOutConnection(recv->videoReceive.back());
  recv->videoReceive.back()->start();
}

void FilterGraph::sendAudioTo(Peer* send, uint16_t port)
{
  Q_ASSERT(send && send->streamID != -1);

  streamer_.addSendAudio(send->streamID, port); // TODO Audioport!

  Filter *framedSource = NULL;
  framedSource = streamer_.getSendFilter(send->streamID, OPUSAUDIO);

  send->audioFramedSource = framedSource;

  audioSend_.back()->addOutConnection(framedSource);
}

void FilterGraph::receiveAudioFrom(Peer* recv, uint16_t port)
{
  Q_ASSERT(recv && recv->streamID != -1);

  streamer_.addReceiveAudio(recv->streamID, port);

  recv->audioSink = streamer_.getReceiveFilter(peers_.back()->streamID, OPUSAUDIO);

  OpusDecoderFilter *decoder = new OpusDecoderFilter(stats_);
  decoder->init(format_);

  recv->audioReceive.push_back(decoder);
  recv->audioSink->addOutConnection(recv->audioReceive.back());
  recv->audioReceive.back()->start();

  bool AEC = true;

  if(audioSend_.size() > 0 && AEC)
  {
    recv->audioReceive.push_back(new SpeexAECFilter(stats_, format_));
    audioSend_.at(0)->addOutConnection(recv->audioReceive.back());
    recv->audioReceive.at(recv->audioReceive.size()-2)->addOutConnection(recv->audioReceive.back());
    recv->audioReceive.back()->start();
  }
  else
  {
    qWarning() << "WARNING: Did not attach echo cancellation";
  }

  peers_.back()->output = new AudioOutput(stats_, recv->streamID);
  peers_.back()->output->initializeAudio(format_);
  AudioOutputDevice* outputModule = recv->output->getOutputModule();

  outputModule->init(recv->audioReceive.back());
}

void FilterGraph::uninit()
{
  streamer_.stop();

  for(Peer* p : peers_)
  {
    if(p != 0)
    {
      destroyPeer(p);
      p = 0;
    }
  }
  peers_.clear();

  destroyFilters(videoSend_);
  destroyFilters(audioSend_);
}

void changeState(Filter* f, bool state)
{
  if(state)
  {
    f->emptyBuffer();
    f->start();
  }
  else
  {
    f->stop();
    f->emptyBuffer();

    while(f->isRunning())
    {
      qSleep(1);
    }
  }
}

void FilterGraph::mic(bool state)
{

  if(audioSend_.size() > 0)
  {
    if(!state)
    {
      qDebug() << "Stopping microphone";
      audioSend_.at(0)->stop();
    }
    else
    {
      qDebug() << "Starting microphone";
      audioSend_.at(0)->start();
    }
  }
}

void FilterGraph::camera(bool state)
{
  if(videoSend_.size() > 0)
  {
    if(!state)
    {
      qDebug() << "Stopping camera";
      videoSend_.at(0)->stop();
    }
    else
    {
      qDebug() << "Starting camera";
      videoSend_.at(0)->start();
    }
  }
}

void FilterGraph::running(bool state)
{
  for(Filter* f : videoSend_)
  {
    changeState(f, state);
  }
  for(Filter* f : audioSend_)
  {
    changeState(f, state);
  }

  for(Peer* p : peers_)
  {
    if(p->audioFramedSource)
    {
      changeState(p->audioFramedSource, state);
    }
    if(p->videoFramedSource)
    {
      changeState(p->videoFramedSource, state);
    }
    for(Filter* f : p->audioReceive)
    {
      changeState(f, state);
    }
    for(Filter* f : p->videoReceive)
    {
      changeState(f, state);
    }
  }
}

void FilterGraph::destroyFilters(std::vector<Filter*>& filters)
{
  qDebug() << "Destroying filter graph with" << filters.size() << "filters.";
  for( Filter *f : filters )
  {
    delete f;
  }

  filters.clear();
}

void FilterGraph::destroyPeer(Peer* peer)
{
  if(peer->audioFramedSource)
  {
    audioSend_.back()->removeOutConnection(peer->audioFramedSource);
    //peer->audioFramedSource is destroyed by RTPStreamer
    peer->audioFramedSource = 0;
  }
  if(peer->videoFramedSource)
  {
    videoSend_.back()->removeOutConnection(peer->videoFramedSource);
    //peer->videoFramedSource is destroyed by RTPStreamer
    peer->videoFramedSource = 0;
  }
  destroyFilters(peer->audioReceive);
  destroyFilters(peer->videoReceive);

  if(peer->output)
  {
    delete peer->output;
    peer->output = 0;
  }
  delete peer;
}
