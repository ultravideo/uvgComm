#include "filtergraph.h"

#include "video/camerafilter.h"
#include "video/kvazaarfilter.h"
#include "video/rgb32toyuv.h"
#include "video/openhevcfilter.h"
#include "video/yuvtorgb32.h"
#include "video/displayfilter.h"
#include "video/dshowcamerafilter.h"
#include "audio/audiocapturefilter.h"
#include "audio/audiooutputdevice.h"
#include "audio/audiooutput.h"
#include "audio/opusencoderfilter.h"
#include "audio/opusdecoderfilter.h"
#include "audio/speexaecfilter.h"
#include "common.h"

#include <QSettings>
#include <QDebug>

FilterGraph::FilterGraph():
  peers_(),
  videoSend_(),
  audioSend_(),
  selfView_(NULL),
  stats_(NULL),
  format_()
{
  // TODO negotiate these values with all included filters and SDP
  // TODO move these to settings and manage them automatically
  format_.setSampleRate(48000);
  format_.setChannelCount(1);
  format_.setSampleSize(16);
  format_.setSampleType(QAudioFormat::SignedInt);
  format_.setByteOrder(QAudioFormat::LittleEndian);
  format_.setCodec("audio/pcm");
}

void FilterGraph::init(VideoWidget* selfView, StatisticsInterface* stats)
{
  Q_ASSERT(stats);

  stats_ = stats;
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

    // update selfview in case camera format has changed
    initSelfView(selfView_);

    // if we are in a call, initiate kvazaar and connect peers. Otherwise add it late.
    if(peers_.size() != 0)
    {
      initVideoSend();

      // reconnect all videosends to streamers
      for(Peer* peer : peers_)
      {
        if(peer != NULL)
        {
          videoSend_.back()->addOutConnection(peer->videoFramedSource);
        }
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
  for(Peer* peer : peers_)
  {
    if(peer != NULL)
    {
      peer->audioFramedSource->updateSettings();
      peer->videoFramedSource->updateSettings();

      // decode and display settings
      for(auto video : peer->videoReceive)
      {
        video->updateSettings();
      }
      // decoding and playback settings
      for(auto audio : peer->audioReceive)
      {
        audio->updateSettings();
      }
    }
  }
}

void FilterGraph::initSelfView(VideoWidget *selfView)
{
  qDebug() << "Iniating camera and selfview";

  if(videoSend_.size() > 0)
  {
    destroyFilters(videoSend_);
  }

  // Sending video graph
  // TODO: move to settings or remove one of these
  if(DSHOW_ENABLED)
  {
    addToGraph(std::shared_ptr<Filter>(new DShowCameraFilter("", stats_)), videoSend_);
  }
  else
    addToGraph(std::shared_ptr<Filter>(new CameraFilter("", stats_)), videoSend_);

  if(selfView)
  {
    // connect selfview to camera
    DisplayFilter* selfviewFilter = new DisplayFilter("Self_", stats_, selfView, 1111);
    // the self view rotation depends on which conversions are use as some of the optimizations
    // do the mirroring. Note: mirroring is slow with Qt
    selfviewFilter->setProperties(true, videoSend_.at(0)->outputType() == RGB32VIDEO);
    addToGraph(std::shared_ptr<Filter>(selfviewFilter), videoSend_);
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

  addToGraph(std::shared_ptr<Filter>(new KvazaarFilter("", stats_)), videoSend_, 0);
}

void FilterGraph::initAudioSend()
{
  // Do this before adding participants, otherwise AEC filter wont get attached
  addToGraph(std::shared_ptr<Filter>(new AudioCaptureFilter("", format_, stats_)), audioSend_);
  addToGraph(std::shared_ptr<Filter>(new OpusEncoderFilter("", format_, stats_)), audioSend_, 0);
}

bool FilterGraph::addToGraph(std::shared_ptr<Filter> filter,
                             std::vector<std::shared_ptr<Filter> > &graph,
                             unsigned int connectIndex)
{
  // // check if we need some sort of conversion and connect to index
  if(graph.size() > 0 && connectIndex <= graph.size() - 1)
  {
    if(graph.at(connectIndex)->outputType() != filter->inputType())
    {
      qDebug() << "Filter output and input do not match for" << graph.at(connectIndex)->getName()
               << "->" << filter->getName() << "Trying to find an existing conversion:" << graph.at(connectIndex)->outputType()
               << "to" << filter->inputType();

      Q_ASSERT(graph.at(connectIndex)->outputType() != NONE);

      if(graph.at(connectIndex)->outputType() == RGB32VIDEO &&
         filter->inputType() == YUVVIDEO)
      {
        qDebug() << "Found RGB32 to YUV conversion needed";
        addToGraph(std::shared_ptr<Filter>(new RGB32toYUV("", stats_)), graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == YUVVIDEO &&
              filter->inputType() == RGB32VIDEO)
      {
        qDebug() << "Found RGB32 to YUV conversion needed";
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_)), graph, connectIndex);
      }
      else
      {
        qWarning() << "WARNING: Could not find conversion for filter";
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = graph.size() - 1;
    }
    connectFilters(filter, graph.at(connectIndex));
  }

  graph.push_back(filter);
  filter->init();
  filter->start();
  qDebug() << "Added, iniated and started" << filter->getName();
  return true;
}

bool FilterGraph::connectFilters(std::shared_ptr<Filter> filter, std::shared_ptr<Filter> previous)
{
  Q_ASSERT(filter != NULL && previous != NULL);
  qDebug() << "Connecting filters:" << previous->getName() << "->" << filter->getName();

  if(previous->outputType() != filter->inputType())
  {
    qWarning() << "WARNING: The connecting filter output and input DO NOT MATCH";
    return false;
  }
  previous->addOutConnection(filter);
  return true;
}

void FilterGraph::checkParticipant(uint32_t sessionID)
{
  Q_ASSERT(stats_);
  Q_ASSERT(sessionID);

  qDebug() << "Checking participant with session ID:" << sessionID;

  if(peers_.size() >= sessionID)
  {
    if(peers_.at(sessionID - 1) != NULL)
    {
      qDebug() << "Filter graph: Peer exists";
      return;
    }
    else
    {
      qDebug() << "Filter graph: Replacing old participant with session ID:" << sessionID;
      peers_.at(sessionID - 1) = new Peer;
    }
  }
  else
  {
    while(peers_.size() < sessionID)
    {
      peers_.push_back(NULL);
    }
    peers_.at(sessionID - 1) = new Peer();

    qDebug() << "Filter graph: Adding participant to end";
  }

  peers_.at(sessionID - 1)->output = 0;
  peers_.at(sessionID - 1)->audioFramedSource = 0;
  peers_.at(sessionID - 1)->videoFramedSource = 0;
}

void FilterGraph::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoFramedSource);

  qDebug() << "Adding send video for peer:" << sessionID;

  // make sure we are generating video
  if(videoSend_.size() < 4)
  {
    initVideoSend();
  }

  // add participant if necessary
  checkParticipant(sessionID - 1);

  if(peers_.at(sessionID - 1)->videoFramedSource)
  {
    qWarning() << "Warning: We are already sending video to participant.";
    return;
  }

  peers_.at(sessionID - 1)->videoFramedSource = videoFramedSource;

  videoSend_.back()->addOutConnection(videoFramedSource);
}

void FilterGraph::receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink, VideoWidget *view)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoSink);

  // add participant if necessary
  checkParticipant(sessionID);

  if(!peers_.at(sessionID - 1)->videoReceive.empty())
  {
    qWarning() << "Warning: We are receiving video from this participant:" << sessionID;
    return;
  }
  addToGraph(videoSink, peers_.at(sessionID - 1)->videoReceive);
  addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(QString::number(sessionID) + "_", stats_)),
             peers_.at(sessionID - 1)->videoReceive, 0);

  addToGraph(std::shared_ptr<Filter>(new DisplayFilter(QString::number(sessionID) + "_", stats_, view, sessionID)),
             peers_.at(sessionID - 1)->videoReceive, 1);
}

void FilterGraph::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioFramedSource);

  // just in case it is wanted later. AEC filter has to be attached
  if(audioSend_.size() == 0)
  {
    initAudioSend();
  }

  // add participant if necessary
  checkParticipant(sessionID - 1);

  if(peers_.at(sessionID - 1)->audioFramedSource)
  {
    qWarning() << "Warning: We are sending audio from to participant:" << sessionID;
    return;
  }

  peers_.at(sessionID - 1)->audioFramedSource = audioFramedSource;

  audioSend_.back()->addOutConnection(audioFramedSource);
}

void FilterGraph::receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioSink);

  // just in case it is wanted later. AEC filter has to be attached
  if(AEC_ENABLED && audioSend_.size() == 0)
  {
    initAudioSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if(!peers_.at(sessionID - 1)->audioReceive.empty())
  {
    qWarning() << "Warning: We are receiving video from this participant:" << sessionID;
    return;
  }
  addToGraph(audioSink, peers_.at(sessionID - 1)->audioReceive);
  addToGraph(std::shared_ptr<Filter>(new OpusDecoderFilter(QString::number(sessionID) + "_", format_, stats_)),
                                     peers_.at(sessionID - 1)->audioReceive, 0);

  if(audioSend_.size() > 0 && AEC_ENABLED)
  {
    addToGraph(std::shared_ptr<Filter>(new SpeexAECFilter(QString::number(sessionID) + "_", stats_, format_)),
               peers_.at(sessionID - 1)->audioReceive, 1);

    //connect to capture filter to remove echo
    audioSend_.at(0)->addOutConnection(peers_.at(sessionID - 1)->audioReceive.back());
  }
  else
  {
    qWarning() << "WARNING: Did not attach echo cancellation";
  }

  peers_.at(sessionID - 1)->output = new AudioOutput(stats_, sessionID);
  peers_.at(sessionID - 1)->output->initializeAudio(format_);
  AudioOutputDevice* outputModule = peers_.at(sessionID - 1)->output->getOutputModule();

  outputModule->init(peers_.at(sessionID - 1)->audioReceive.back());
}

void FilterGraph::uninit()
{
  removeAllParticipants();

  destroyFilters(videoSend_);
  destroyFilters(audioSend_);
}

void FilterGraph::removeAllParticipants()
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
}

void changeState(std::shared_ptr<Filter> f, bool state)
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
  for(std::shared_ptr<Filter> f : videoSend_)
  {
    changeState(f, state);
  }
  for(std::shared_ptr<Filter> f : audioSend_)
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
      for(std::shared_ptr<Filter> f : p->audioReceive)
      {
        changeState(f, state);
      }
      for(std::shared_ptr<Filter> f : p->videoReceive)
      {
        changeState(f, state);
      }
    }
  }
}

void FilterGraph::destroyFilters(std::vector<std::shared_ptr<Filter> > &filters)
{
  if(filters.size() != 0)
  {
    qDebug() << "Destroying filter graph with" << filters.size() << "filters.";
  }
  for( std::shared_ptr<Filter> f : filters )
  {
    changeState(f, false);
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

  destroyFilters(peer->audioReceive);
  destroyFilters(peer->videoReceive);

  if(peer->output)
  {
    delete peer->output;
    peer->output = 0;
  }
  delete peer;
}

void FilterGraph::removeParticipant(uint32_t sessionID)
{
  qDebug() << "Removing peer:" << sessionID << "/" << peers_.size();
  Q_ASSERT(sessionID < peers_.size());
  if(peers_.at(sessionID - 1) != NULL)
    destroyPeer(peers_.at(sessionID - 1));
  peers_.at(sessionID - 1) = NULL;

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
