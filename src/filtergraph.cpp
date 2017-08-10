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
#include "dshowcamerafilter.h"

#include <QSettings>
#include <QDebug>

#include "common.h"

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
  format_.setChannelCount(1);
  format_.setSampleSize(16);
  format_.setSampleType(QAudioFormat::SignedInt);
  format_.setByteOrder(QAudioFormat::LittleEndian);
  format_.setCodec("audio/pcm");
}

void FilterGraph::init(VideoWidget* selfView)
{
  selfView_ = selfView;

  initSelfView(selfView);
}

void FilterGraph::updateSettings()
{
  QSettings settings;
  // if the video format has changed so that we need different conversions
  if(videoFormat_ != settings.value("video/InputFormat").toString())
  {
    qDebug() << "The video format has been changed from " << videoFormat_
             << "to" << settings.value("video/InputFormat").toString() << "Video send graph has to be reconstructed.";

    initSelfView(selfView_);
    if(peers_.size() != 0)
    {
      initVideoSend();

      // reconnect all videosends to streamers
      for(Peer* peer : peers_)
      {
        videoSend_.back()->addOutConnection(peer->videoFramedSource);
      }
    }
  }
  else
  {
    for(auto filter : videoSend_)
    {
      filter->updateSettings();
    }
  }
  for(auto filter : audioSend_)
  {
    filter->updateSettings();
  }
  for(auto peer : peers_)
  {
    peer->audioFramedSource->updateSettings();
    peer->videoFramedSource->updateSettings();

    peer->audioSink->updateSettings();
    peer->videoSink->updateSettings();

    // currently does nothing, but maybe later we decide to add some settings
    // to our receive such as hos much we want to decode and stuff
    for(auto video : peer->videoReceive)
    {
      video->updateSettings();
    }
    for(auto audio : peer->audioReceive)
    {
      audio->updateSettings();
    }
  }


  // update selfview in case camera format has changed
}

void FilterGraph::initSelfView(VideoWidget *selfView)
{
  qDebug() << "Iniating camera and selfview";

  if(videoSend_.size() > 0)
  {
    destroyFilters(videoSend_);
  }

  // Sending video graph
  if(DSHOW_ENABLED)
  {
    DShowCameraFilter* dshow = new DShowCameraFilter("", stats_);
    dshow->init();
    videoSend_.push_back(dshow);
    videoSend_.back()->start();
  }
  else
    videoSend_.push_back(new CameraFilter("", stats_));

  if(selfView)
  {
    // TODO filters could automatically detect output of previous filter
    // and add a conversion filter afterwards if needed

    QSettings settings;
    videoFormat_ = settings.value("video/InputFormat").toString();
    if(videoFormat_ == "I420")
    {
      qDebug() << "Adding YUV to RGB32 conversion after camera for selfview";
      videoSend_.push_back(new YUVtoRGB32("", stats_));
      videoSend_.at(videoSend_.size() - 2)->addOutConnection(videoSend_.back()); // attach to camera
      videoSend_.back()->start();
    }
    else
    {
      qDebug() << "Input already RGB32, no need for conversion to selfview";
    }

    // connect selfview to camera
    DisplayFilter* selfviewFilter = new DisplayFilter("Self_", stats_, selfView, 1111);
    selfviewFilter->setProperties(true);
    videoSend_.push_back(selfviewFilter);
    videoSend_.at(videoSend_.size() - 2)->addOutConnection(videoSend_.back());
    videoSend_.back()->start();
  }
}

void FilterGraph::initVideoSend()
{
  qDebug() << "Iniating video send";

  if(videoSend_.size() == 0)
  {
    qDebug() << "WARNING: Camera was not iniated for video send";
    initSelfView(selfView_);
  }
  else if(videoSend_.size() > 3)
  {
    qDebug() << "WARNING: Too many filters in videosend";
    destroyFilters(videoSend_);
  }

  QSettings settings;
  if(settings.value("video/InputFormat").toString() != "I420")
  {
    qDebug() << "Adding RGB32 to YUV conversion after camera";
    videoSend_.push_back(new RGB32toYUV("", stats_));
    videoSend_.at(0)->addOutConnection(videoSend_.back()); // attach to camera
    videoSend_.back()->start();
  }
  else
  {
    qDebug() << "Input already YUV, no need for conversion to kvazaar";
  }

  KvazaarFilter* kvz = new KvazaarFilter("", stats_);
  kvz->init();
  videoSend_.push_back(kvz);
  videoSend_.at(videoSend_.size() - 2)->addOutConnection(videoSend_.back());
  videoSend_.back()->start();
}

void FilterGraph::initAudioSend()
{
  // Do this before adding participants, otherwise AEC filter wont get attached

  AudioCaptureFilter* capture = new AudioCaptureFilter("", format_, stats_);
  capture->init();
  audioSend_.push_back(capture);

  OpusEncoderFilter *encoder = new OpusEncoderFilter("", format_, stats_);
  encoder->init();
  audioSend_.push_back(encoder);
  audioSend_.at(audioSend_.size() - 2)->addOutConnection(audioSend_.back());
  audioSend_.back()->start();
}

bool FilterGraph::addFilter(Filter* filter, std::vector<Filter*>& graph)
{
  if(graph.back()->outputType() != filter->inputType())
  {
    qDebug() << "Filter output and input do not match. Trying to find an existing conversion";

    if(graph.back()->outputType() == RGB32VIDEO &&
       filter->inputType() == YUVVIDEO)
    {
      addFilter(new RGB32toYUV("", stats_), graph);
    }
    else if(graph.back()->outputType() == YUVVIDEO &&
            filter->inputType() == RGB32VIDEO)
    {
      addFilter(new YUVtoRGB32("", stats_), graph);
    }
    else
    {
      qWarning() << "WARNING: Could not find conversion";
      return false;
    }
  }
  graph.push_back(filter);
  connectFilters(filter, graph.back());
  return true;
}

bool FilterGraph::connectFilters(Filter* filter, Filter* previous)
{
  if(previous->outputType() != filter->inputType())
  {
    qWarning() << "WARNING: The connecting filter output and input DO NOT MATCH";
    return false;
  }

  previous->addOutConnection(filter);
  filter->start();
}


void FilterGraph::checkParticipant(int16_t id)
{
  Q_ASSERT(stats_);
  Q_ASSERT(id > -1);

  qDebug() << "Checking participant with id:" << id;

  if(peers_.size() > (unsigned int)id)
  {
    if(peers_.at(id) != NULL)
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
    while(peers_.size() < (unsigned int)id)
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
  if(videoSend_.size() < 4)
  {
    initVideoSend();
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

  OpenHEVCFilter* decoder =  new OpenHEVCFilter(QString::number(id) + "_", stats_);
  decoder->init();
  peers_.at(id)->videoReceive.push_back(decoder);
  peers_.at(id)->videoSink->addOutConnection(peers_.at(id)->videoReceive.back());
  peers_.at(id)->videoReceive.back()->start();

  peers_.at(id)->videoReceive.push_back(new YUVtoRGB32(QString::number(id) + "_", stats_));
  peers_.at(id)->videoReceive.at(peers_.at(id)->videoReceive.size()-2)
      ->addOutConnection(peers_.at(id)->videoReceive.back());
  peers_.at(id)->videoReceive.back()->start();

  peers_.at(id)->videoReceive.push_back(new DisplayFilter(QString::number(id) + "_", stats_, view, id));
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

  // just in case it is wanted later. AEC filter has to be attached
  if(AEC_ENABLED && audioSend_.size() == 0)
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

  OpusDecoderFilter *decoder = new OpusDecoderFilter(QString::number(id) + "_", format_, stats_);
  decoder->init();

  peers_.at(id)->audioReceive.push_back(decoder);
  peers_.at(id)->audioSink->addOutConnection(peers_.at(id)->audioReceive.back());
  peers_.at(id)->audioReceive.back()->start();

  if(audioSend_.size() > 0 && AEC_ENABLED)
  {
    peers_.at(id)->audioReceive.push_back(new SpeexAECFilter(QString::number(id) + "_", stats_, format_));
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
    if(p != NULL)
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
    if(p != NULL)
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
}

void FilterGraph::destroyFilters(std::vector<Filter*>& filters)
{
  if(filters.size() != 0)
    qDebug() << "Destroying filter graph with" << filters.size() << "filters.";
  for( Filter *f : filters )
  {
    changeState(f, false);
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
    peer->audioFramedSource->stop();
    peer->audioFramedSource = 0;

    if(AEC_ENABLED)
    {
      audioSend_.at(0)->removeOutConnection(peer->audioReceive.back());
    }
  }
  if(peer->videoFramedSource)
  {
    videoSend_.back()->removeOutConnection(peer->videoFramedSource);
    peer->videoFramedSource->stop();
    //peer->videoFramedSource is destroyed by RTPStreamer
    peer->videoFramedSource = 0;
  }

  if(peer->audioSink)
  {
    peer->audioSink->removeOutConnection(peer->audioReceive.front());
    peer->audioSink->stop();
  }

  if(peer->videoSink)
  {
    peer->videoSink->removeOutConnection(peer->videoReceive.front());
    peer->videoSink->stop();
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


void FilterGraph::removeParticipant(int16_t id)
{
  qDebug() << "Removing peer:" << id +1 << "/" << peers_.size();
  Q_ASSERT(id < peers_.size());
  if(peers_.at(id) != NULL)
    destroyPeer(peers_.at(id));
  peers_.at(id) = NULL;

    // destroy send graphs if this was the last peer
  bool peerPresent = false;
  for(auto peer : peers_)
  {
    if (peer != NULL)
      peerPresent = true;
  }

  if(!peerPresent)
  {
    destroyFilters(videoSend_);
    initSelfView(selfView_); // restore the self view.
    destroyFilters(audioSend_);
  }
}


void FilterGraph::print()
{
  QString audioDotFile = "digraph AudioGraph {\r\n";

  for(auto f : audioSend_)
  {
    audioDotFile += f->printOutputs();
  }

  for(unsigned int i = 0; i <peers_.size(); ++i)
  {
    if(peers_.at(i) != NULL)
    {
      for(auto f : peers_.at(i)->audioReceive)
      {
        audioDotFile += f->printOutputs();
      }

      audioDotFile += peers_.at(i)->audioSink->printOutputs();
      audioDotFile += peers_.at(i)->audioFramedSource->printOutputs();
    }
  }
  audioDotFile += "}";

  QString videoDotFile = "digraph VideoGraph {\r\n";

  for(auto f : videoSend_)
  {
    videoDotFile += f->printOutputs();
  }

  for(unsigned int i = 0; i <peers_.size(); ++i)
  {
    if(peers_.at(i) != NULL)
    {
      for(auto f : peers_.at(i)->videoReceive)
      {
        videoDotFile += f->printOutputs();
      }

      videoDotFile += peers_.at(i)->videoSink->printOutputs();
      videoDotFile += peers_.at(i)->videoFramedSource->printOutputs();
    }
  }
  videoDotFile += "}";

  QString aFilename="audiograph.dot";
  QFile aFile( aFilename );
  if ( aFile.open(QIODevice::WriteOnly) )
  {
      QTextStream stream( &aFile );
      stream << audioDotFile << endl;
  }

  QString vFilename="videograph.dot";
  QFile vFile( vFilename );
  if ( vFile.open(QIODevice::WriteOnly) )
  {
      QTextStream stream( &vFile );
      stream << videoDotFile << endl;
  }
}
