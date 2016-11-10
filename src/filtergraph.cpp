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


// Didn't find sleep in QCore
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

void FilterGraph::checkParticipant(int16_t id)
{
  Q_ASSERT(stats_);
  Q_ASSERT(id > -1);

  qDebug() << "Checking participant with id:" << id;

  if(peers_.size() > (unsigned int)id)
  {
    if(peers_.at(id) != 0)
    {
      qDebug() << "Filter graph: Peer exists";
      return;
    }
    else
    {
      qDebug() << "Filter graph: Replacing old participant with id:" << id;
      peers_.at(id) = new Peer;
    }
  }
  else
  {
    while(peers_.size() < id)
    {
      peers_.push_back(0);
    }
    peers_.push_back(new Peer);

    qDebug() << "Filter graph: Adding participant to end";
  }

  peers_.at(id)->output = 0;
  peers_.at(id)->audioFramedSource = 0;
  peers_.at(id)->videoFramedSource = 0;
  peers_.at(id)->audioSink = 0;
  peers_.at(id)->videoSink = 0;
}

void FilterGraph::sendVideoto(int16_t id, Filter *videoFramedSource)
{
  Q_ASSERT(id > -1);
  Q_ASSERT(videoFramedSource);

  qDebug() << "Adding send video for peer:" << id;

  // make sure we are generating video
  if(videoSend_.size() < 3)
  {
    initVideoSend(resolution_);
  }

  // add participant if necessary
  checkParticipant(id);

  if(peers_.at(id)->videoFramedSource)
  {
    qWarning() << "Warning: We are already sending video to participant.";
    return;
  }

  peers_.at(id)->videoFramedSource = videoFramedSource;

  videoSend_.back()->addOutConnection(videoFramedSource);
}

void FilterGraph::receiveVideoFrom(int16_t id, Filter *videoSink, VideoWidget *view)
{
  Q_ASSERT(id > -1);
  Q_ASSERT(videoSink);

  // add participant if necessary
  checkParticipant(id);

  if(peers_.at(id)->videoSink)
  {
    qWarning() << "Warning: We are receiving video from this participant:" << id;
    return;
  }

  peers_.at(id)->videoSink = videoSink;

  OpenHEVCFilter* decoder =  new OpenHEVCFilter(stats_);
  decoder->init();
  peers_.at(id)->videoReceive.push_back(decoder);
  peers_.at(id)->videoSink->addOutConnection(peers_.at(id)->videoReceive.back());
  peers_.at(id)->videoReceive.back()->start();

  peers_.at(id)->videoReceive.push_back(new YUVtoRGB32(stats_));
  peers_.at(id)->videoReceive.at(peers_.at(id)->videoReceive.size()-2)
      ->addOutConnection(peers_.at(id)->videoReceive.back());
  peers_.at(id)->videoReceive.back()->start();

  peers_.at(id)->videoReceive.push_back(new DisplayFilter(stats_, view, id));
  peers_.at(id)->videoReceive.at(peers_.at(id)->videoReceive.size()-2)
      ->addOutConnection(peers_.at(id)->videoReceive.back());
  peers_.at(id)->videoReceive.back()->start();
}

void FilterGraph::sendAudioTo(int16_t id, Filter* audioFramedSource)
{
  Q_ASSERT(id > -1);
  Q_ASSERT(audioFramedSource);

  // just in case it is wanted later. AEC filter has to be attached
  if(audioSend_.size() == 0)
  {
    initAudioSend();
  }

  // add participant if necessary
  checkParticipant(id);

  if(peers_.at(id)->audioFramedSource)
  {
    qWarning() << "Warning: We are sending audio from to participant:" << id;
    return;
  }

  peers_.at(id)->audioFramedSource = audioFramedSource;

  audioSend_.back()->addOutConnection(audioFramedSource);
}

void FilterGraph::receiveAudioFrom(int16_t id, Filter* audioSink)
{
  Q_ASSERT(id > -1);
  Q_ASSERT(audioSink);

  bool AEC = true;

  // just in case it is wanted later. AEC filter has to be attached
  if(AEC && audioSend_.size() == 0)
  {
    initAudioSend();
  }

  // add participant if necessary
  checkParticipant(id);

  if(peers_.at(id)->audioSink)
  {
    qWarning() << "Warning: We are receiving video from this participant:" << id;
    return;
  }
  peers_.at(id)->audioSink = audioSink;

  OpusDecoderFilter *decoder = new OpusDecoderFilter(stats_);
  decoder->init(format_);

  peers_.at(id)->audioReceive.push_back(decoder);
  peers_.at(id)->audioSink->addOutConnection(peers_.at(id)->audioReceive.back());
  peers_.at(id)->audioReceive.back()->start();

  if(audioSend_.size() > 0 && AEC)
  {
    peers_.at(id)->audioReceive.push_back(new SpeexAECFilter(stats_, format_));
    audioSend_.at(0)->addOutConnection(peers_.at(id)->audioReceive.back());
    peers_.at(id)->audioReceive.at(peers_.at(id)->audioReceive.size()-2)
        ->addOutConnection(peers_.at(id)->audioReceive.back());
    peers_.at(id)->audioReceive.back()->start();
  }
  else
  {
    qWarning() << "WARNING: Did not attach echo cancellation";
  }

  peers_.at(id)->output = new AudioOutput(stats_, id);
  peers_.at(id)->output->initializeAudio(format_);
  AudioOutputDevice* outputModule = peers_.at(id)->output->getOutputModule();

  outputModule->init(peers_.at(id)->audioReceive.back());
}

void FilterGraph::uninit()
{
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
