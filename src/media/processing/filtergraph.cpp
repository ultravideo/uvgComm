#include "filtergraph.h"

#include "media/processing/camerafilter.h"
#include "media/processing/screensharefilter.h"
#include "media/processing/kvazaarfilter.h"

#include "media/processing/openhevcfilter.h"

#include "media/processing/rgb32toyuv.h"
#include "media/processing/yuvtorgb32.h"
#include "media/processing/yuyvtoyuv420.h"
#include "media/processing/yuyvtorgb32.h"
#include "media/processing/halfrgbfilter.h"

#include "media/processing/displayfilter.h"
#include "media/processing/scalefilter.h"
#include "media/processing/audiocapturefilter.h"
#include "media/processing/opusencoderfilter.h"
#include "media/processing/opusdecoderfilter.h"
#include "media/processing/dspfilter.h"
#include "media/processing/audiomixerfilter.h"
#include "media/processing/audiooutputfilter.h"

#include "media/processing/audioframebuffer.h"

#include "media/resourceallocator.h"

#include "ui/gui/videointerface.h"

#include "speexaec.h"
#include "audiomixer.h"

#include "settingskeys.h"
#include "global.h"
#include "common.h"
#include "logger.h"

#include <QSettings>
#include <QFile>
#include <QTextStream>

#include <chrono>
#include <thread>


// speex DSP settings

// We limit the gain so background noises don't start coming through in a quiet
// audio stream.

// The default Speex AGC volume seems to be 1191182336 which is around 55%.
// 50% seems to be completely quiet

// The input volume we want to normilize so the differences in mics and distance
// from mic don't affect the sound as much
const int32_t AUDIO_INPUT_VOLUME = 1191182336; // default
const int AUDIO_INPUT_GAIN = 10; // dB

// The output we want to normalize if we have multiple participants, two people
// speaking at the same time should not sound very loud and only one person
// speaking very quiet.
const int32_t AUDIO_OUTPUT_VOLUME = INT32_MAX - INT32_MAX/4;
const int AUDIO_OUTPUT_GAIN = 20; // dB


FilterGraph::FilterGraph(): QObject(),
  peers_(),
  cameraGraph_(),
  screenShareGraph_(),
  audioInputGraph_(),
  audioOutputGraph_(),
  selfviewFilter_(nullptr),
  hwResources_(nullptr),
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


void FilterGraph::init(QList<VideoInterface *> selfViews, StatisticsInterface* stats,
                       std::shared_ptr<ResourceAllocator> hwResources)
{
  Q_ASSERT(stats);

  uninit();

  quitting_ = false;

  stats_ = stats;

  hwResources_ = hwResources;

  selfviewFilter_ =
      std::shared_ptr<DisplayFilter>(new DisplayFilter("Self", stats_, hwResources_, selfViews, 1111));

  initSelfView();
}


void FilterGraph::updateVideoSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);
  // if the video format has changed so that we need different conversions

  QString wantedVideoFormat = settings.value(SettingsKey::videoInputFormat).toString();
  if(videoFormat_ != wantedVideoFormat)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
                                    "Video format changed. Reconstructing video send graph.",
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
    // camera and conversions
    for(auto& filter : cameraGraph_)
    {
      filter->updateSettings();
    }
  }

  // screen share and conversions
  for (auto& filter : screenShareGraph_)
  {
    filter->updateSettings();
  }

  // send and receive graphs for each individual peer
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


void FilterGraph::updateAutomaticSettings()
{
  if (hwResources_)
  {
    hwResources_->updateSettings();
  }
}


void FilterGraph::initSelfView()
{
  Logger::getLogger()->printNormal(this, "Iniating camera");

  QSettings settings(settingsFile, settingsFileFormat);
  videoFormat_ = settings.value(SettingsKey::videoInputFormat).toString();

  if (cameraGraph_.size() > 0)
  {
    destroyFilters(cameraGraph_);
  }

  // Sending video graph
  if (!addToGraph(std::shared_ptr<Filter>(new CameraFilter("", stats_, hwResources_)), cameraGraph_))
  {
    // camera failed
    Logger::getLogger()->printError(this, "Failed to add camera. Does it have supported formats?");
    return; // TODO: return false that we failed so user can fix camera selection
  }

  // create screen share filter, but it is stopped at the beginning
  if (screenShareGraph_.size() == 0)
  {
    if (addToGraph(std::shared_ptr<Filter>(new ScreenShareFilter("", stats_, hwResources_)),
                   screenShareGraph_))
    {
      screenShareGraph_.at(0)->stop();
    }
  }

  if (selfviewFilter_)
  {
    Logger::getLogger()->printNormal(this, "Iniating self view");
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

    // We don't do horizontal mirroring if we are using screen sharing, but that
    // is changed later.
    // Note: mirroring is slow with Qt

    std::shared_ptr<Filter> resizeFilter = std::shared_ptr<Filter>(new HalfRGBFilter("", stats_, hwResources_));

    addToGraph(resizeFilter, cameraGraph_, cameraGraph_.size() - 1);
    addToGraph(resizeFilter, screenShareGraph_, cameraGraph_.size() - 1);

    selfviewFilter_->setHorizontalMirroring(true);

    addToGraph(selfviewFilter_, cameraGraph_, cameraGraph_.size() - 1);
    addToGraph(selfviewFilter_, screenShareGraph_, cameraGraph_.size() - 1);
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Self view filter has not been set");
  }

  selectVideoSource();
}


void FilterGraph::initVideoSend()
{
  Logger::getLogger()->printNormal(this, "Iniating video send");

  if(cameraGraph_.size() == 0)
  {
    Logger::getLogger()->printProgramWarning(this, "Camera was not iniated for video send");
    initSelfView();
  }
  else if(cameraGraph_.size() > 3)
  {
    Logger::getLogger()->printProgramError(this, "Too many filters in videosend");
    destroyFilters(cameraGraph_);
  }

  std::shared_ptr<Filter> kvazaar = std::shared_ptr<Filter>(new KvazaarFilter("", stats_, hwResources_));
  addToGraph(kvazaar, cameraGraph_, 0);
  addToGraph(kvazaar, screenShareGraph_, 0);
}


void FilterGraph::initializeAudio(bool opus)
{
  // Do this before adding participants, otherwise AEC filter wont get attached
  addToGraph(std::shared_ptr<Filter>(new AudioCaptureFilter("", format_, stats_, hwResources_)),
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

  // Do everything (AGC, AEC, denoise, dereverb) for input expect provide AEC reference
  std::shared_ptr<DSPFilter> dspProcessor =
      std::shared_ptr<DSPFilter>(new DSPFilter("", stats_, hwResources_, aec_, format_,
                                               false, true, true, true, true, AUDIO_INPUT_VOLUME, AUDIO_INPUT_GAIN));

  addToGraph(dspProcessor, audioInputGraph_, (unsigned int)audioInputGraph_.size() - 1);

  if (opus)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusEncoderFilter("", format_, stats_, hwResources_)),
               audioInputGraph_, (unsigned int)audioInputGraph_.size() - 1);
  }

  // Provide echo reference and do AGC once more so conference calls will have
  // good volume levels.
  std::shared_ptr<DSPFilter> echoReference =
      std::make_shared<DSPFilter>("", stats_, hwResources_, aec_, format_,
                                  true, false, false, false, true, AUDIO_OUTPUT_VOLUME, AUDIO_OUTPUT_GAIN);

  addToGraph(echoReference, audioOutputGraph_);

  std::shared_ptr<AudioOutputFilter> audioOutput =
      std::make_shared<AudioOutputFilter>("", stats_, hwResources_, format_);

  addToGraph(audioOutput, audioOutputGraph_, (unsigned int)audioOutputGraph_.size() - 1);
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
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
                                      "Filter output and input do not match. Finding conversion",
                                      {"Connection"}, {graph.at(connectIndex)->getName() + "->" + filter->getName()});

      if (graph.at(connectIndex)->outputType() == DT_NONE)
      {
        Logger::getLogger()->printProgramError(this, "The previous filter has no output!");
        return false;
      }

      // TODO: Check the out connections of connected filter for an already existing conversion.

      if(graph.at(connectIndex)->outputType() == DT_RGB32VIDEO &&
         filter->inputType() == DT_YUV420VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding RGB32 to YUV420 conversion");
        addToGraph(std::shared_ptr<Filter>(new RGB32toYUV("", stats_, hwResources_)),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == DT_YUV420VIDEO &&
              filter->inputType() == DT_RGB32VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding YUV420 to RGB32 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_, hwResources_)),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == DT_YUYVVIDEO &&
              filter->inputType() == DT_YUV420VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding YUYV to YUV420 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUYVtoYUV420("", stats_, hwResources_)),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == DT_YUYVVIDEO &&
              filter->inputType() == DT_RGB32VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding YUYV to RGB32 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUYVtoRGB32("", stats_, hwResources_)),
                   graph, connectIndex);
      }
      else
      {
        Logger::getLogger()->printProgramError(this, "Could not find conversion for filter.");
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = (unsigned int)graph.size() - 1;
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
    Logger::getLogger()->printProgramError(this, "Failed to init filter");
    return false;
  }
  return true;
}


bool FilterGraph::connectFilters(std::shared_ptr<Filter> previous, std::shared_ptr<Filter> filter)
{
  Q_ASSERT(filter != nullptr && previous != nullptr);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "FilterGraph", "Connecting filters",
                                  {"Connection"}, {previous->getName() + " -> " + filter->getName()});

  if(previous->outputType() != filter->inputType())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "FilterGraph",
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

  Logger::getLogger()->printNormal(this, "Adding send video", {"SessionID"}, 
                                   QString::number(sessionID));

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
  addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(sessionID, stats_, hwResources_)), *graph, 0);

  std::shared_ptr<DisplayFilter> displayFilter =
      std::shared_ptr<DisplayFilter>(new DisplayFilter(QString::number(sessionID),
                                                stats_, hwResources_, {view}, sessionID));

  addToGraph(displayFilter, *graph, 1);
}


void FilterGraph::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioFramedSource);

  // just in case it is wanted later. AEC filter has to be attached
  if(audioInputGraph_.size() == 0)
  {
    initializeAudio(audioFramedSource->inputType() == DT_OPUSAUDIO);
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

  if (audioSink->outputType() == DT_OPUSAUDIO)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusDecoderFilter(sessionID, format_, stats_, hwResources_)),
               *graph, (unsigned int)graph->size() - 1);
  }

  std::shared_ptr<AudioMixerFilter> audioMixer =
      std::make_shared<AudioMixerFilter>(QString::number(sessionID), stats_, hwResources_, sessionID, mixer_);

  addToGraph(audioMixer, *graph, (unsigned int)graph->size() - 1);

  if (!audioOutputGraph_.empty())
  {
    // connects audio reception to audio output
    connectFilters(graph->back(), audioOutputGraph_.front());
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Audio output not initialized "
                                                 "when adding audio reception");
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
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}


void FilterGraph::mic(bool state)
{
  if(audioInputGraph_.size() > 0)
  {
    if(!state)
    {
      Logger::getLogger()->printNormal(this, "Stopping microphone");
      audioInputGraph_.at(0)->stop();
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Starting microphone");
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
      Logger::getLogger()->printNormal(this, "Stopping camera");
      cameraGraph_.at(0)->stop();
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Starting camera", "Output Type",
                                       QString::number(cameraGraph_.at(0)->outputType()));
      selfviewFilter_->setHorizontalMirroring(true);
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
      Logger::getLogger()->printNormal(this, "Starting to share the screen");

      // We don't want to flip selfview horizontally when sharing the screen.
      // This way the self view is an accurate representation of what the others
      // will view. With camera on the other hand, we want it to mirror the user.
      selfviewFilter_->setHorizontalMirroring(false);
      screenShareGraph_.at(0)->start();
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Not sharing the screen");
      screenShareGraph_.at(0)->stop();
    }
  }
}


void FilterGraph::selectVideoSource()
{
  if (settingEnabled(SettingsKey::screenShareStatus))
  {
    Logger::getLogger()->printNormal(this, "Enabled screen sharing in filter graph");
    camera(false);
    screenShare(true);
  }
  else if (settingEnabled(SettingsKey::cameraStatus))
  {
    Logger::getLogger()->printNormal(this, "Enabled camera in filter graph");
    screenShare(false);
    camera(true);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "No video in filter graph");

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
    Logger::getLogger()->printNormal(this, "Destroying filter in one graph",
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
  Logger::getLogger()->printNormal(this, "Destroying peer from Filter Graph");

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
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
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
