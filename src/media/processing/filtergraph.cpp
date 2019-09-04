#include "filtergraph.h"

#include "media/processing/camerafilter.h"
#include "media/processing/kvazaarfilter.h"
#include "media/processing/rgb32toyuv.h"
#include "media/processing/openhevcfilter.h"
#include "media/processing/yuvtorgb32.h"
#include "media/processing/displayfilter.h"
#include "media/processing/scalefilter.h"
#include "media/processing/audiocapturefilter.h"
#include "media/processing/audiooutputdevice.h"
#include "media/processing/audiooutput.h"
#include "media/processing/opusencoderfilter.h"
#include "media/processing/opusdecoderfilter.h"
#include "media/processing/speexaecfilter.h"

#include "ui/gui/videointerface.h"

#include "global.h"
#include "common.h"

#include <QSettings>

FilterGraph::FilterGraph():
  peers_(),
  videoProcessing_(),
  audioProcessing_(),
  selfView_(nullptr),
  stats_(nullptr),
  conversionIndex_(0),
  format_(),
  quitting_(false)
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

void FilterGraph::init(VideoInterface* selfView, StatisticsInterface* stats)
{
  Q_ASSERT(stats);

  stats_ = stats;
  selfView_ = selfView;

  initSelfView(selfView);
}

void FilterGraph::updateSettings()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  // if the video format has changed so that we need different conversions
  if(videoFormat_ != settings.value("video/InputFormat").toString())
  {
    qDebug() << "Settings, FilterGraph :" << "The video format has been changed from " << videoFormat_
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
        if(peer != nullptr)
        {
          for (auto senderFilter : peer->videoSenders)
          {
            videoProcessing_.back()->addOutConnection(senderFilter);
          }
        }
      }
    }
  }
  else
  {
    for(auto filter : videoProcessing_)
    {
      filter->updateSettings();
    }
  }
  for(auto filter : audioProcessing_)
  {
    filter->updateSettings();
  }
  for(Peer* peer : peers_)
  {
    if(peer != nullptr)
    {
      for (auto senderFilter : peer->videoSenders)
      {
        senderFilter->updateSettings();
      }
      for (auto senderFilter : peer->audioSenders)
      {
        senderFilter->updateSettings();
      }

      // decode and display settings
      for(auto videoReceivers : peer->videoReceivers)
      {
        for (auto filter : *videoReceivers)
        {
          filter->updateSettings();
        }
      }

      for(auto audioReceivers : peer->audioReceivers)
      {
        for (auto filter : *audioReceivers)
        {
          filter->updateSettings();
        }
      }
    }
  }
}

void FilterGraph::initSelfView(VideoInterface *selfView)
{
  qDebug() << "Iniating, FilterGraph : " << "Iniating camera and selfview";

  if(videoProcessing_.size() > 0)
  {
    destroyFilters(videoProcessing_);
  }

  // Sending video graph

  addToGraph(std::shared_ptr<Filter>(new CameraFilter("", stats_)), videoProcessing_);

  if(selfView)
  {
    // connect scaling filter
    // TODO: not useful if it does not support YUV. Testing needed to verify
    /*
    ScaleFilter* scaler = new ScaleFilter("Self_", stats_);
    scaler->setResolution(selfView->size());
    addToGraph(std::shared_ptr<Filter>(scaler), videoSend_);
    */

    // connect selfview to camera
    DisplayFilter* selfviewFilter = new DisplayFilter("Self_", stats_, selfView, 1111);
    // the self view rotation depends on which conversions are use as some of the optimizations
    // do the mirroring. Note: mirroring is slow with Qt
    selfviewFilter->setProperties(true, videoProcessing_.at(0)->outputType() == RGB32VIDEO);
    addToGraph(std::shared_ptr<Filter>(selfviewFilter), videoProcessing_);
  }
}

void FilterGraph::initVideoSend()
{
  qDebug() << "Session construction," << "FilterGraph :" << "Iniating video send";

  if(videoProcessing_.size() == 0)
  {
    qDebug() << "WARNING: FilterGraph : Camera was not iniated for video send";
    initSelfView(selfView_);
  }
  else if(videoProcessing_.size() > 3)
  {
    qDebug() << "WARNING: FilterGraph : Too many filters in videosend";
    destroyFilters(videoProcessing_);
  }

  addToGraph(std::shared_ptr<Filter>(new KvazaarFilter("", stats_)), videoProcessing_, 0);
}

void FilterGraph::initAudioSend()
{
  // Do this before adding participants, otherwise AEC filter wont get attached
  addToGraph(std::shared_ptr<Filter>(new AudioCaptureFilter("", format_, stats_)), audioProcessing_);
  addToGraph(std::shared_ptr<Filter>(new OpusEncoderFilter("", format_, stats_)), audioProcessing_, 0);
}

bool FilterGraph::addToGraph(std::shared_ptr<Filter> filter,
                             GraphSegment &graph,
                             unsigned int connectIndex)
{
  // // check if we need some sort of conversion and connect to index
  if(graph.size() > 0 && connectIndex <= graph.size() - 1)
  {
    if(graph.at(connectIndex)->outputType() != filter->inputType())
    {
      qDebug() << "FilterGraph : Filter output and input do not match for" << graph.at(connectIndex)->getName()
               << "->" << filter->getName() << "Trying to find an existing conversion:"
               << graph.at(connectIndex)->outputType() << "to" << filter->inputType();

      Q_ASSERT(graph.at(connectIndex)->outputType() != NONE);

      // TODO: Check the out connections of connected filter for an already existing conversion.

      if(graph.at(connectIndex)->outputType() == RGB32VIDEO &&
         filter->inputType() == YUV420VIDEO)
      {
        qDebug() << "FilterGraph : Found RGB32 to YUV conversion needed";
        addToGraph(std::shared_ptr<Filter>(new RGB32toYUV("", stats_, conversionIndex_)), graph, connectIndex);
        ++conversionIndex_;
      }
      else if(graph.at(connectIndex)->outputType() == YUV420VIDEO &&
              filter->inputType() == RGB32VIDEO)
      {
        qDebug() << "FilterGraph : Found RGB32 to YUV conversion needed";
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_, conversionIndex_)), graph, connectIndex);
        ++conversionIndex_;
      }
      else
      {
        printDebug(DEBUG_WARNING, "FilterGraph", DC_STARTUP,
                   "Could not find conversion for filter.");
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = graph.size() - 1;
    }
    connectFilters(filter, graph.at(connectIndex));
  }

  graph.push_back(filter);
  if(filter->init())
  {
    filter->start();
  }
  qDebug() << "FilterGraph : Added, iniated and started" << filter->getName();
  return true;
}

bool FilterGraph::connectFilters(std::shared_ptr<Filter> filter, std::shared_ptr<Filter> previous)
{
  Q_ASSERT(filter != nullptr && previous != nullptr);
  qDebug() << "FilterGraph : Connecting filters:" << previous->getName() << "->" << filter->getName();

  if(previous->outputType() != filter->inputType())
  {
    printDebug(DEBUG_WARNING, "FilterGraph", DC_STARTUP,
               "The connecting filter output and input DO NOT MATCH.");
    return false;
  }
  previous->addOutConnection(filter);
  return true;
}

void FilterGraph::checkParticipant(uint32_t sessionID)
{
  Q_ASSERT(stats_);
  Q_ASSERT(sessionID);

  //qDebug() << "FilterGraph : Checking participant with session ID:" << sessionID;

  if(peers_.size() >= sessionID)
  {
    if(peers_.at(sessionID - 1) != nullptr)
    {
      qDebug() << "FilterGraph: Peer exists for session ID:" << sessionID;
      return;
    }
    else
    {
      qDebug() << "FilterGraph: Replacing old participant with session ID:" << sessionID;
      peers_.at(sessionID - 1) = new Peer;
    }
  }
  else
  {
    while(peers_.size() < sessionID)
    {
      peers_.push_back(nullptr);
    }
    peers_.at(sessionID - 1) = new Peer();

    qDebug() << "FilterGraph: Adding participant to end with sessionID:" << sessionID;
  }

  peers_.at(sessionID - 1)->output = nullptr;
  peers_.at(sessionID - 1)->audioSenders.clear();
  peers_.at(sessionID - 1)->videoSenders.clear();
}

void FilterGraph::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoFramedSource);

  qDebug() << "Session construction, FilterGraph: Adding send video for peer:" << sessionID;

  // make sure we are generating video
  if(videoProcessing_.size() < 4)
  {
    initVideoSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  peers_.at(sessionID - 1)->videoSenders.push_back(videoFramedSource);

  videoProcessing_.back()->addOutConnection(videoFramedSource);
  videoFramedSource->start();
}

void FilterGraph::receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                   VideoInterface *view)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoSink);

  checkParticipant(sessionID);

  std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
  peers_.at(sessionID - 1)->videoReceivers.push_back(graph);

  addToGraph(videoSink, *graph);
  addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(QString::number(sessionID) + "_", stats_)),
             *graph, 0);

  addToGraph(std::shared_ptr<Filter>(new DisplayFilter(QString::number(sessionID) + "_", stats_,
                                                       view, sessionID)),
             *graph, 1);
}

void FilterGraph::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioFramedSource);

  // just in case it is wanted later. AEC filter has to be attached
  if(audioProcessing_.size() == 0)
  {
    initAudioSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);


  peers_.at(sessionID - 1)->audioSenders.push_back(audioFramedSource);

  audioProcessing_.back()->addOutConnection(audioFramedSource);
  audioFramedSource->start();
}

void FilterGraph::receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioSink);

  // just in case it is wanted later. AEC filter has to be attached
  if(AEC_ENABLED && audioProcessing_.size() == 0)
  {
    initAudioSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
  peers_.at(sessionID - 1)->audioReceivers.push_back(graph);

  addToGraph(audioSink, *graph);
  addToGraph(std::shared_ptr<Filter>(new OpusDecoderFilter(QString::number(sessionID) + "_", format_, stats_)),
                                     *graph, 0);

  if(audioProcessing_.size() > 0 && AEC_ENABLED)
  {
    addToGraph(std::shared_ptr<Filter>(new SpeexAECFilter(QString::number(sessionID) + "_", stats_, format_)),
               *graph, 1);

    //connect to capture filter to remove echo
    audioProcessing_.at(0)->addOutConnection(graph->back());
  }
  else
  {
    printDebug(DEBUG_WARNING, "FilterGraph", DC_ADD_MEDIA,
               "Did not attach echo cancellation");
  }

  peers_.at(sessionID - 1)->output = new AudioOutput(stats_, sessionID);
  peers_.at(sessionID - 1)->output->initializeAudio(format_);
  AudioOutputDevice* outputModule = peers_.at(sessionID - 1)->output->getOutputModule();

  outputModule->init(graph->back());
}

void FilterGraph::uninit()
{
  quitting_ = true;
  removeAllParticipants();

  destroyFilters(videoProcessing_);
  destroyFilters(audioProcessing_);
}

void FilterGraph::removeAllParticipants()
{
  for(unsigned int i = 0; i < peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      removeParticipant(i + 1);
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
  if(audioProcessing_.size() > 0)
  {
    if(!state)
    {
      qDebug() << "FilterGraph : Stopping microphone";
      audioProcessing_.at(0)->stop();
    }
    else
    {
      qDebug() << "FilterGraph : Starting microphone";
      audioProcessing_.at(0)->start();
    }
  }
}

void FilterGraph::camera(bool state)
{
  if(videoProcessing_.size() > 0)
  {
    if(!state)
    {
      qDebug() << "FilterGraph : Stopping camera";
      videoProcessing_.at(0)->stop();
    }
    else
    {
      qDebug() << "FilterGraph : Starting camera";
      videoProcessing_.at(0)->start();
    }
  }
}

void FilterGraph::running(bool state)
{
  for(std::shared_ptr<Filter> f : videoProcessing_)
  {
    changeState(f, state);
  }
  for(std::shared_ptr<Filter> f : audioProcessing_)
  {
    changeState(f, state);
  }

  for(Peer* p : peers_)
  {
    if(p != nullptr)
    {
      for (auto senderFilter : p->audioSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto senderFilter : p->videoSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto graph : p->audioReceivers)
      {
        for(std::shared_ptr<Filter> f : *graph)
        {
          changeState(f, state);
        }
      }

      for (auto graph : p->videoReceivers)
      {
        for(std::shared_ptr<Filter> f : *graph)
        {
          changeState(f, state);
        }
      }
    }
  }
}

void FilterGraph::destroyFilters(std::vector<std::shared_ptr<Filter> > &filters)
{
  if(filters.size() != 0)
  {
    qDebug() << "Closing, FilterGraph :"
             << "Destroying filter graph with" << filters.size() << "filters.";
  }
  for( std::shared_ptr<Filter> f : filters )
  {
    changeState(f, false);
  }

  filters.clear();
}

void FilterGraph::destroyPeer(Peer* peer)
{
  qDebug() << "Remove participant, Filter Graph: Destroying peer from Filter Graph";

  for (auto audioSender : peer->audioSenders)
  {
    audioProcessing_.back()->removeOutConnection(audioSender);
    //peer->audioFramedSource is destroyed by RTPStreamer
    changeState(audioSender, false);
    audioSender = nullptr;

    if(AEC_ENABLED)
    {
      for (auto graph : peer->audioReceivers)
      {
        audioProcessing_.at(0)->removeOutConnection(graph->back());
      }
    }
  }
  for (auto videoSender : peer->videoSenders)
  {
    videoProcessing_.back()->removeOutConnection(videoSender);
    changeState(videoSender, false);
    //peer->videoFramedSource is destroyed by RTPStreamer
    videoSender = nullptr;
  }
  for (auto graph : peer->audioReceivers)
  {
    destroyFilters(*graph);
  }

  for (auto graph : peer->videoReceivers)
  {
    destroyFilters(*graph);
  }

  if(peer->output)
  {
    delete peer->output;
    peer->output = nullptr;
  }
  delete peer;
}

void FilterGraph::removeParticipant(uint32_t sessionID)
{
  qDebug() << "Remove participant, FilterGraph : Removing peer:" << sessionID << "/" << peers_.size();
  Q_ASSERT(sessionID <= peers_.size());
  if(peers_.at(sessionID - 1) != nullptr)
    destroyPeer(peers_.at(sessionID - 1));
  peers_.at(sessionID - 1) = nullptr;

    // destroy send graphs if this was the last peer
  bool peerPresent = false;
  for(auto peer : peers_)
  {
    if (peer != nullptr)
      peerPresent = true;
  }

  if(!peerPresent)
  {
    destroyFilters(videoProcessing_);
    if (!quitting_)
    {
      initSelfView(selfView_); // restore the self view.
    }

    destroyFilters(audioProcessing_);
  }
}

void FilterGraph::print()
{
  QString audioDotFile = "digraph AudioGraph {\r\n";

  for(auto f : audioProcessing_)
  {
    audioDotFile += f->printOutputs();
  }

  for(unsigned int i = 0; i <peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      for(auto graph : peers_.at(i)->audioReceivers)
      {
        for (auto filter : *graph)
        {
          audioDotFile += filter->printOutputs();
        }

      }
      for (auto audioSender : peers_.at(i)->audioSenders)
      {
        audioDotFile += audioSender->printOutputs();
      }
    }
  }
  audioDotFile += "}";

  QString videoDotFile = "digraph VideoGraph {\r\n";

  for(auto f : videoProcessing_)
  {
    videoDotFile += f->printOutputs();
  }

  for(unsigned int i = 0; i <peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      for(auto graph : peers_.at(i)->videoReceivers)
      {
        for (auto filter : *graph)
        {
          videoDotFile += filter->printOutputs();
        }
      }

      for (auto videoSender : peers_.at(i)->videoSenders)
      {
        audioDotFile += videoSender->printOutputs();
      }
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