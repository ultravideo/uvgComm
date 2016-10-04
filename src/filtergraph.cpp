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

  if(wantsAudio && audioSend_.size() == 0)
  {
    initAudioSend();
  }

  if(wantsVideo && videoSend_.size() < 3)
  {
    initVideoSend(resolution_);
  }

  peers_.push_back(new Peer);

  PeerID peer = streamer_.addPeer(ip);

  if(peer == -1)
  {
    qCritical() << "Error creating RTP peer";
    return peer;
  }

  if(wantsAudio)
  {
    streamer_.addSendAudio(peer, port + 1000); // TODO Audioport!

    Filter *framedSource = NULL;
    framedSource = streamer_.getSendFilter(peer, OPUSAUDIO);

    peers_.back()->audioFramedSource = framedSource;

    audioSend_.back()->addOutConnection(framedSource);
  }

  if(sendsAudio)
  {
    streamer_.addReceiveAudio(peer, port + 1000);

    std::vector<Filter*>& audioReceive = peers_.back()->audioReceive;

    Filter* rtpSink = NULL;
    rtpSink = streamer_.getReceiveFilter(peer, OPUSAUDIO);

    audioReceive.push_back(rtpSink);

    OpusDecoderFilter *decoder = new OpusDecoderFilter(stats_);
    decoder->init(format_);

    audioReceive.push_back(decoder);
    audioReceive.at(audioReceive.size()-2)
        ->addOutConnection(audioReceive.back());
    audioReceive.back()->start();

    peers_.back()->output = new AudioOutput(stats_, peer);
    peers_.back()->output->initializeAudio(format_);
    AudioOutputDevice* outputModule = peers_.back()->output->getOutputModule();

    outputModule->init(audioReceive.back());
  }

  if(wantsVideo)
  {
    streamer_.addSendVideo(peer, port);

    Filter *framedSource = NULL;
    framedSource = streamer_.getSendFilter(peer, HEVCVIDEO);

    peers_.back()->videoFramedSource = framedSource;

    videoSend_.back()->addOutConnection(framedSource);
  }

  if(sendsVideo && view != NULL)
  {
    streamer_.addReceiveVideo(peer, port);

    std::vector<Filter*>& videoReceive = peers_.back()->videoReceive;

    // Receiving video graph
    Filter* rtpSink = NULL;
    rtpSink = streamer_.getReceiveFilter(peer, HEVCVIDEO);

    videoReceive.push_back(rtpSink);

    OpenHEVCFilter* decoder =  new OpenHEVCFilter(stats_);
    decoder->init();
    videoReceive.push_back(decoder);
    videoReceive.at(videoReceive.size()-2)->addOutConnection(videoReceive.back());
    videoReceive.back()->start();

    videoReceive.push_back(new YUVtoRGB32(stats_));
    videoReceive.at(videoReceive.size()-2)->addOutConnection(videoReceive.back());
    videoReceive.back()->start();

    videoReceive.push_back(new DisplayFilter(stats_, view, peer));
    videoReceive.at(videoReceive.size()-2)->addOutConnection(videoReceive.back());
    videoReceive.back()->start();
  }
  else if(view == NULL)
    qWarning() << "Warn: wanted to receive video, but no view available";

  qDebug() << "Participant has been successfully added to call.";

  return peer;
}

void FilterGraph::uninit()
{
  deconstruct();
}

void FilterGraph::deconstruct()
{
  stop();

  for(Peer* p : peers_)
  {
    if(p != 0)
    {
      delete p;
    }
    p = 0;
  }
  peers_.clear();

  destroyFilters(videoSend_);
  destroyFilters(audioSend_);
}

void FilterGraph::restart()
{
  for(Filter* f : videoSend_)
    f->start();

  streamer_.start();
}

void FilterGraph::stop()
{
  for(Filter* f : videoSend_)
  {
    f->stop();
    f->emptyBuffer();
  }

  for(Filter* f : audioSend_)
  {
    f->stop();
    f->emptyBuffer();
  }
  streamer_.stop();
}

void FilterGraph::destroyFilters(std::vector<Filter*>& filters)
{
  qDebug() << "Destroying filter graph with" << filters.size() << "filters.";
  for( Filter *f : filters )
  {
    f->stop();
    delete f;
  }

  filters.clear();
}

void FilterGraph::destroyPeer(Peer* peer)
{
  if(peer->audioFramedSource)
  {
    delete peer->audioFramedSource;
    peer->audioFramedSource = 0;
  }
  if(peer->videoFramedSource)
  {
    delete peer->videoFramedSource;
    peer->videoFramedSource = 0;
  }

  destroyFilters(peer->audioReceive);
  destroyFilters(peer->audioReceive);

  if(peer->output)
  {
    delete peer->output;
    peer->output = 0;
  }
}
