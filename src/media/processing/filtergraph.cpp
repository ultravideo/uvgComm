#include "filtergraph.h"

#include "media/processing/camerafilter.h"
#include "media/processing/screensharefilter.h"
#include "media/processing/kvazaarfilter.h"
#include "media/processing/rgb32toyuv.h"
#include "media/processing/openhevcfilter.h"
#include "media/processing/yuvtorgb32.h"
#include "media/processing/displayfilter.h"
#include "media/processing/scalefilter.h"
#include "media/processing/audiocapturefilter.h"
#include "media/processing/opusencoderfilter.h"
#include "media/processing/opusdecoderfilter.h"
#include "media/processing/dspfilter.h"
#include "media/processing/audiomixerfilter.h"
#include "media/processing/audiooutputfilter.h"

#include "ui/gui/videointerface.h"

#include "speexaec.h"
#include "audiomixer.h"

#include "settingskeys.h"
#include "global.h"
#include "common.h"

#include <QSettings>
#include <QFile>
#include <QTextStream>

FilterGraph::FilterGraph(): QObject(),
  peers_(),
  cameraGraph_(),
  screenShareGraph_(),
  audioInputGraph_(),
  audioOutputGraph_(),
  selfviewFilter_(nullptr),
  stats_(nullptr),
  format_(),
  videoFormat_(""),
  quitting_(false),
  aec_(nullptr),
  mixer_()
{
  // TODO negotiate these values with all included filters and SDP
  // TODO move these to settings and manage them automatically

  // 48000 should be used with opus, since opus is able to downsample when needed
  format_ = createAudioFormat(1, 48000);
}


void FilterGraph::init(VideoInterface* selfView, StatisticsInterface* stats)
{
  Q_ASSERT(stats);

  stats_ = stats;
  selfviewFilter_ =
      std::shared_ptr<DisplayFilter>(new DisplayFilter("Self", stats_, selfView, 1111));

  initSelfView();
}


void FilterGraph::updateVideoSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);
  // if the video format has changed so that we need different conversions

  QString wantedVideoFormat = settings.value(SettingsKey::videoInputFormat).toString();
  if(videoFormat_ != wantedVideoFormat)
  {
    printDebug(DEBUG_NORMAL, this, "Video format changed. Reconstructing video send graph.",
               {"Previous format", "New format"},
               {videoFormat_, settings.value(SettingsKey::videoInputFormat).toString()});


    // update selfview in case camera format has changed
    initSelfView();

    // if we are in a call, initiate kvazaar and connect peers. Otherwise add it late.
    if(peers_.size() != 0)
    {
      initVideoSend();

      // reconnect all videosends to streamers
      for(auto& peer : peers_)
      {
        if(peer.second != nullptr)
        {
          for (auto& senderFilter : peer.second->videoSenders)
          {
            cameraGraph_.back()->addOutConnection(senderFilter);
          }
        }
      }
    }
  }
  else
  {
    for(auto& filter : cameraGraph_)
    {
      filter->updateSettings();
    }
  }

  for (auto& filter : screenShareGraph_)
  {
    filter->updateSettings();
  }

  for(auto& peer : peers_)
  {
    if(peer.second != nullptr)
    {
      for (auto& senderFilter : peer.second->videoSenders)
      {
        senderFilter->updateSettings();
      }

      // decode and display settings
      for(auto& videoReceivers : peer.second->videoReceivers)
      {
        for (auto& filter : *videoReceivers)
        {
          filter->updateSettings();
        }
      }
    }
  }

  selectVideoSource();
}


void FilterGraph::updateAudioSettings()
{
  for (auto& filter : audioInputGraph_)
  {
    filter->updateSettings();
  }

  for (auto& filter : audioOutputGraph_)
  {
    filter->updateSettings();
  }

  for (auto& peer : peers_)
  {
    if (peer.second != nullptr)
    {
      for (auto& senderFilter : peer.second->audioSenders)
      {
        senderFilter->updateSettings();
      }

      for (auto& audioReceivers : peer.second->audioReceivers)
      {
        for (auto& filter : *audioReceivers)
        {
          filter->updateSettings();
        }
      }
    }
  }

  if (aec_)
  {
    aec_->updateSettings();
  }

  mic(settingEnabled(SettingsKey::micStatus));
}


void FilterGraph::initSelfView()
{
  printNormal(this, "Iniating camera and selfview");

  QSettings settings(settingsFile, settingsFileFormat);
  videoFormat_ = settings.value(SettingsKey::videoInputFormat).toString();

  if (cameraGraph_.size() > 0)
  {
    destroyFilters(cameraGraph_);
  }

  // Sending video graph
  if (!addToGraph(std::shared_ptr<Filter>(new CameraFilter("", stats_)), cameraGraph_))
  {
    // camera failed
    printError(this, "Failed to add camera. Does it have supported formats.");
    return; // TODO: return false that we failed so user can fix camera selection
  }

  // create screen share filter, but it is stopped at the beginning
  if (screenShareGraph_.size() == 0)
  {
    if (addToGraph(std::shared_ptr<Filter>(new ScreenShareFilter("", stats_)),
                   screenShareGraph_))
    {
      screenShareGraph_.at(0)->stop();
    }
  }

  if (selfviewFilter_)
  {
    // connect scaling filter
    // TODO: not useful if it does not support YUV. Testing needed to verify
    /*
    ScaleFilter* scaler = new ScaleFilter("Self", stats_);
    scaler->setResolution(selfView->size());
    addToGraph(std::shared_ptr<Filter>(scaler), videoSend_);
    */

    // Connect selfview to camera and screen sharing. The self view vertical mirroring
    // depends on which conversions are used.

    // TODO: Figure out what is going on. There is probably a bug in one of the
    // optimization that flip the video. This however removes the need for an
    // additional flip for some reason saving CPU.

    // We dont do horizontal mirroring if we are using screen sharing, but that
    // is changed later.
    // Note: mirroring is slow with Qt

    selfviewFilter_->setProperties(true, cameraGraph_.at(0)->outputType() == RGB32VIDEO);

    addToGraph(selfviewFilter_, cameraGraph_);
    addToGraph(selfviewFilter_, screenShareGraph_);
  }

  selectVideoSource();
}


void FilterGraph::initVideoSend()
{
  printNormal(this, "Iniating video send");

  if(cameraGraph_.size() == 0)
  {
    printProgramWarning(this, "Camera was not iniated for video send");
    initSelfView();
  }
  else if(cameraGraph_.size() > 3)
  {
    printProgramError(this, "Too many filters in videosend");
    destroyFilters(cameraGraph_);
  }

  std::shared_ptr<Filter> kvazaar = std::shared_ptr<Filter>(new KvazaarFilter("", stats_));
  addToGraph(kvazaar, cameraGraph_, 0);
  addToGraph(cameraGraph_.back(), screenShareGraph_, 0);
}


void FilterGraph::initializeAudio(bool opus)
{
  // Do this before adding participants, otherwise AEC filter wont get attached
  addToGraph(std::shared_ptr<Filter>(new AudioCaptureFilter("", format_, stats_)),
             audioInputGraph_);

  if (aec_ == nullptr)
  {
    aec_ = std::make_shared<SpeexAEC>(format_);
    aec_->init();
  }

  // mixer helps mix the incoming audio streams into one output stream
  if (mixer_ == nullptr)
  {
    mixer_ = std::make_shared<AudioMixer>();
  }

  std::shared_ptr<DSPFilter> dspProcessor =
      std::shared_ptr<DSPFilter>(new DSPFilter("", stats_, DSP_PROCESSOR, aec_, format_));

  addToGraph(dspProcessor, audioInputGraph_, audioInputGraph_.size() - 1);

  if (opus)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusEncoderFilter("", format_, stats_)),
               audioInputGraph_, audioInputGraph_.size() - 1);
  }

  std::shared_ptr<DSPFilter> echoReference =
      std::make_shared<DSPFilter>("", stats_, ECHO_FRAME_PROVIDER, aec_, format_);

  addToGraph(echoReference, audioOutputGraph_);

  std::shared_ptr<AudioOutputFilter> audioOutput =
      std::make_shared<AudioOutputFilter>("", stats_, format_);

  addToGraph(audioOutput, audioOutputGraph_, audioOutputGraph_.size() - 1);
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
      printDebug(DEBUG_NORMAL, this, "Filter output and input do not match. Finding conversion",
                 {"Connection"}, {graph.at(connectIndex)->getName() + "->" + filter->getName()});

      Q_ASSERT(graph.at(connectIndex)->outputType() != NONE);

      // TODO: Check the out connections of connected filter for an already existing conversion.

      if(graph.at(connectIndex)->outputType() == RGB32VIDEO &&
         filter->inputType() == YUV420VIDEO)
      {
        printNormal(this, "Adding RGB32 to YUV conversion");
        addToGraph(std::shared_ptr<Filter>(new RGB32toYUV("", stats_)),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == YUV420VIDEO &&
              filter->inputType() == RGB32VIDEO)
      {
        printNormal(this, "Adding YUV to RGB32 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_)),
                   graph, connectIndex);
      }
      else
      {
        printProgramError(this, "Could not find conversion for filter.");
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = graph.size() - 1;
    }
    connectFilters(graph.at(connectIndex), filter);
  }

  graph.push_back(filter);
  if(filter->init())
  {
    filter->start();
  }
  else
  {
    return false;
  }
  return true;
}


bool FilterGraph::connectFilters(std::shared_ptr<Filter> previous, std::shared_ptr<Filter> filter)
{
  Q_ASSERT(filter != nullptr && previous != nullptr);

  printDebug(DEBUG_NORMAL, "FilterGraph", "Connecting filters",
              {"Connection"}, {previous->getName() + " -> " + filter->getName()});

  if(previous->outputType() != filter->inputType())
  {
    printDebug(DEBUG_WARNING, "FilterGraph", 
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

  if(peers_.find(sessionID) != peers_.end())
  {
    if(peers_[sessionID] != nullptr)
    { 
      return;
    }
    else
    {
      peers_[sessionID] = new Peer();
    }
  }
  else
  {
    peers_[sessionID] = new Peer();
  }

  peers_[sessionID]->audioSenders.clear();
  peers_[sessionID]->videoSenders.clear();
}


void FilterGraph::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoFramedSource);

  printNormal(this, "Adding send video", {"SessionID"}, QString::number(sessionID));

  // make sure we are generating video
  if(cameraGraph_.size() < 4)
  {
    initVideoSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  peers_[sessionID]->videoSenders.push_back(videoFramedSource);

  cameraGraph_.back()->addOutConnection(videoFramedSource);
  videoFramedSource->start();
}


void FilterGraph::receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                   VideoInterface *view)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoSink);

  checkParticipant(sessionID);

  std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
  peers_[sessionID]->videoReceivers.push_back(graph);

  addToGraph(videoSink, *graph);
  addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(sessionID, stats_)), *graph, 0);

  std::shared_ptr<DisplayFilter> displayFilter =
      std::shared_ptr<DisplayFilter>(new DisplayFilter(QString::number(sessionID),
                                                stats_, view, sessionID));

  addToGraph(displayFilter, *graph, 1);
}


void FilterGraph::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioFramedSource);

  // just in case it is wanted later. AEC filter has to be attached
  if(audioInputGraph_.size() == 0)
  {
    initializeAudio(audioFramedSource->inputType() == OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);

  peers_[sessionID]->audioSenders.push_back(audioFramedSource);

  audioInputGraph_.back()->addOutConnection(audioFramedSource);
  audioFramedSource->start();

  mic(settingEnabled(SettingsKey::micStatus));
}


void FilterGraph::receiveAudioFrom(uint32_t sessionID,
                                   std::shared_ptr<Filter> audioSink)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioSink);


  // add participant if necessary
  checkParticipant(sessionID);

  std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
  peers_[sessionID]->audioReceivers.push_back(graph);

  addToGraph(audioSink, *graph);

  if (audioSink->outputType() == OPUSAUDIO)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusDecoderFilter(sessionID, format_, stats_)),
               *graph, graph->size() - 1);
  }

  std::shared_ptr<AudioMixerFilter> audioMixer =
      std::make_shared<AudioMixerFilter>(QString::number(sessionID), stats_, sessionID, mixer_);

  addToGraph(audioMixer, *graph, graph->size() - 1);

  if (!audioOutputGraph_.empty())
  {
    // connects audio reception to audio output
    connectFilters(graph->back(), audioOutputGraph_.front());
  }
  else
  {
    printProgramError(this, "Audio output not initialized when adding audio reception");
  }
}


void FilterGraph::uninit()
{
  quitting_ = true;
  removeAllParticipants();

  destroyFilters(cameraGraph_);
  destroyFilters(screenShareGraph_);

  destroyFilters(audioInputGraph_);
  destroyFilters(audioOutputGraph_);
}


void FilterGraph::removeAllParticipants()
{
  for(auto& peer : peers_)
  {
    if(peer.second != nullptr)
    {
      removeParticipant(peer.first);
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
  if(audioInputGraph_.size() > 0)
  {
    if(!state)
    {
      printNormal(this, "Stopping microphone");
      audioInputGraph_.at(0)->stop();
    }
    else
    {
      printNormal(this, "Starting microphone");
      audioInputGraph_.at(0)->start();
    }
  }
}


void FilterGraph::camera(bool state)
{
  if(cameraGraph_.size() > 0)
  {
    if(!state)
    {
      printNormal(this, "Stopping camera");
      cameraGraph_.at(0)->stop();
    }
    else
    {
      printNormal(this, "Starting camera");
      selfviewFilter_->setProperties(true, cameraGraph_.at(0)->outputType() == RGB32VIDEO);
      cameraGraph_.at(0)->start();
    }
  }
}


void FilterGraph::screenShare(bool shareState)
{
  if(cameraGraph_.size() > 0 && screenShareGraph_.size() > 0)
  {
    if(shareState)
    {
      printNormal(this, "Starting to share the screen");

      // We don't want to flip selfview horizontally when sharing the screen.
      // This way the self view is an accurate representation of what the others
      // will view. With camera on the other hand, we want it to mirror the user.
      selfviewFilter_->setProperties(false, true);
      screenShareGraph_.at(0)->start();
    }
    else
    {
      printNormal(this, "Not sharing the screen");
      screenShareGraph_.at(0)->stop();
    }
  }
}


void FilterGraph::selectVideoSource()
{
  if (settingEnabled(SettingsKey::screenShareStatus))
  {
    printNormal(this, "Enabled screen sharing in filter graph");
    camera(false);
    screenShare(true);
  }
  else if (settingEnabled(SettingsKey::cameraStatus))
  {
    printNormal(this, "Enabled camera in filter graph");
    screenShare(false);
    camera(true);
  }
  else
  {
    printNormal(this, "No video in filter graph");

    screenShare(false);
    camera(false);

    // maybe some custom image here?
  }
}


void FilterGraph::running(bool state)
{
  for(std::shared_ptr<Filter>& f : cameraGraph_)
  {
    changeState(f, state);
  }

  for(std::shared_ptr<Filter>& f : audioInputGraph_)
  {
    changeState(f, state);
  }

  for(std::shared_ptr<Filter>& f : audioOutputGraph_)
  {
    changeState(f, state);
  }

  if (screenShareGraph_.size() > 0)
  {
    changeState(screenShareGraph_.at(0), state);
  }

  for(auto& peer : peers_)
  {
    if(peer.second != nullptr)
    {
      for (auto& senderFilter : peer.second->audioSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto& senderFilter : peer.second->videoSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto& graph : peer.second->audioReceivers)
      {
        for(std::shared_ptr<Filter>& f : *graph)
        {
          changeState(f, state);
        }
      }

      for (auto& graph : peer.second->videoReceivers)
      {
        for(std::shared_ptr<Filter>& f : *graph)
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
    printNormal(this, "Destroying filter in one graph",
                {"Filter"}, {QString::number(filters.size())});
  }
  for( std::shared_ptr<Filter>& f : filters )
  {
    changeState(f, false);
  }

  filters.clear();
}


void FilterGraph::destroyPeer(Peer* peer)
{
  printNormal(this, "Destroying peer from Filter Graph");

  for (auto& audioSender : peer->audioSenders)
  {
    audioInputGraph_.back()->removeOutConnection(audioSender);
    changeState(audioSender, false);
    audioSender = nullptr;
  }
  for (auto& videoSender : peer->videoSenders)
  {
    cameraGraph_.back()->removeOutConnection(videoSender);
    changeState(videoSender, false);
    videoSender = nullptr;
  }
  for (auto& graph : peer->audioReceivers)
  {
    destroyFilters(*graph);
  }

  for (auto& graph : peer->videoReceivers)
  {
    destroyFilters(*graph);
  }

  delete peer;
}


void FilterGraph::removeParticipant(uint32_t sessionID)
{
  Q_ASSERT(peers_.find(sessionID) != peers_.end());
  if (peers_.find(sessionID) != peers_.end() &&
      peers_[sessionID] != nullptr)
  {
    printDebug(DEBUG_NORMAL, this,
               "Removing peer", {"SessionID", "Remaining sessions"},
               {QString::number(sessionID), QString::number(peers_.size())});

    destroyPeer(peers_[sessionID]);
    peers_[sessionID] = nullptr;

    // destroy send graphs if this was the last peer
    bool peerPresent = false;
    for(auto& peer : peers_)
    {
      if (peer.second != nullptr)
      {
        peerPresent = true;
      }
    }

    if(!peerPresent)
    {
      destroyFilters(cameraGraph_);
      destroyFilters(screenShareGraph_);
      if (!quitting_)
      {
        initSelfView(); // restore the self view.
      }

      destroyFilters(audioInputGraph_);
      destroyFilters(audioOutputGraph_);

      selectVideoSource();
      mic(settingEnabled(SettingsKey::micStatus));
    }
  }
}


QAudioFormat FilterGraph::createAudioFormat(uint8_t channels, uint32_t sampleRate)
{
  QAudioFormat format;

  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleSize(16);
  format.setSampleType(QAudioFormat::SignedInt);
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setCodec("audio/pcm");

  return format;
}
