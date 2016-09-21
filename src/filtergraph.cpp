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
  filters_(),
  videoSendIniated_(false),
  videoEncoderFilter_(0),
  audioEncoderFilter_(0),
  streamer_(stats),
  stats_(stats)
{
  Q_ASSERT(stats);
}

void FilterGraph::init(VideoWidget* selfView, QSize resolution)
{
  //streamer_.setPorts(15555,18888);
  streamer_.start();

  resolution_ = resolution;
  selfView_ = selfView;
  frameRate_ = 30;

  initSender(selfView, resolution);
}

void FilterGraph::initSender(VideoWidget *selfView, QSize resolution)
{
  Q_ASSERT(stats_);
  // Sending video graph
  filters_.push_back(new CameraFilter(stats_, resolution));

  // connect selfview to camera
  DisplayFilter* selfviewFilter = new DisplayFilter(stats_, selfView);
  selfviewFilter->setProperties(true);
  filters_.push_back(selfviewFilter);
  filters_.at(0)->addOutConnection(filters_.back());
  filters_.back()->start();

  filters_.push_back(new RGB32toYUV(stats_));
  filters_.at(0)->addOutConnection(filters_.back());
  filters_.back()->start();

  KvazaarFilter* kvz = new KvazaarFilter(stats_);
  kvz->init(resolution, frameRate_, 1, 0);
  filters_.push_back(kvz);
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
  filters_.back()->start();

  videoEncoderFilter_ = filters_.size() - 1;

  videoSendIniated_ = true;

  // audio filtergraph test
  AudioCaptureFilter* capture = new AudioCaptureFilter(stats_);
  capture->init();
  filters_.push_back(capture);

  OpusEncoderFilter *encoder = new OpusEncoderFilter(stats_);
  encoder->init();
  filters_.push_back(encoder);
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
  filters_.back()->start();

  audioEncoderFilter_ = filters_.size() - 1;
}

ParticipantID FilterGraph::addParticipant(in_addr ip, uint16_t port, VideoWidget* view,
                                          bool wantsAudio, bool sendsAudio,
                                          bool wantsVideo, bool sendsVideo)
{
  Q_ASSERT(stats_);

  //if(port != 0)
  //  streamer_.setPorts(15555, port);

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

    filters_.push_back(framedSource);

    filters_.at(audioEncoderFilter_)->addOutConnection(filters_.back());
    //filters_.back()->start();
  }

  if(sendsAudio)
  {
    streamer_.addReceiveAudio(peer, port + 1000);

    Filter* rtpSink = NULL;
    rtpSink = streamer_.getReceiveFilter(peer, OPUSAUDIO);

    filters_.push_back(rtpSink);
    //filters_.back()->start();

    OpusDecoderFilter *decoder = new OpusDecoderFilter(stats_);
    decoder->init();
    filters_.push_back(decoder);
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();

    AudioOutput* output = new AudioOutput(stats_);
    output->initializeAudio();
    AudioOutputDevice* outputModule = output->getOutputModule();

    outputModule->init(filters_.back());

    outputs_.push_back(output);
  }

  if(wantsVideo)
  {
    streamer_.addSendVideo(peer, port);

    if(!videoSendIniated_)
    {
      initSender(selfView_, resolution_);
    }
    Filter *framedSource = NULL;
    framedSource = streamer_.getSendFilter(peer, HEVCVIDEO);

    filters_.push_back(framedSource);

    filters_.at(videoEncoderFilter_)->addOutConnection(filters_.back());
    //filters_.back()->start();
  }

  if(sendsVideo && view != NULL)
  {
    streamer_.addReceiveVideo(peer, port);

    // Receiving video graph
    Filter* rtpSink = NULL;
    rtpSink = streamer_.getReceiveFilter(peer, HEVCVIDEO);

    filters_.push_back(rtpSink);
    //filters_.back()->start();

    OpenHEVCFilter* decoder =  new OpenHEVCFilter(stats_);
    decoder->init();
    filters_.push_back(decoder);
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();

    filters_.push_back(new YUVtoRGB32(stats_));
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();

    filters_.push_back(new DisplayFilter(stats_, view));
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();
  }
  else if(view == NULL)
    qWarning() << "Warn: wanted to receive video, but no view available";

  return peer;
}

void FilterGraph::uninit()
{
  deconstruct();
}

void FilterGraph::deconstruct()
{
  for( Filter *f : filters_ )
    delete f;

  for( AudioOutput* a : outputs_)
    delete a;

  filters_.clear();
}

void FilterGraph::restart()
{
  for(Filter* f : filters_)
    f->start();

  streamer_.start();
}

void FilterGraph::stop()
{
  for(Filter* f : filters_)
  {
    f->stop();
    f->emptyBuffer();
  }
  streamer_.stop();
}
