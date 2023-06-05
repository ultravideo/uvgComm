#include "filtergraph.h"

#include "media/processing/camerafilter.h"
#include "media/processing/screensharefilter.h"
#include "media/processing/kvazaarfilter.h"
#include "media/processing/roimanualfilter.h"

#include "media/processing/openhevcfilter.h"

#include "media/processing/yuvtorgb32.h"
#include "media/processing/halfrgbfilter.h"
#include "media/processing/libyuvconverter.h"

#include "media/processing/displayfilter.h"
#include "media/processing/audiocapturefilter.h"
#include "media/processing/opusencoderfilter.h"
#include "media/processing/opusdecoderfilter.h"
#include "media/processing/dspfilter.h"
#include "media/processing/audiomixerfilter.h"
#include "media/processing/audiooutputfilter.h"

#ifdef KVAZZUP_HAVE_ONNX_RUNTIME
  #include "media/processing/roifilter.h"
#endif

#include "media/resourceallocator.h"

#include "ui/gui/videointerface.h"

#include "speexaec.h"
#include "audiomixer.h"

#include "settingskeys.h"
#include "common.h"
#include "logger.h"

#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QAudioFormat>

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

void changeState(std::shared_ptr<Filter> f, bool state);

FilterGraph::FilterGraph(): QObject(),
  quitting_(false),
  peers_(),
  hwResources_(nullptr),
  stats_(nullptr),
  cameraGraph_(),
  screenShareGraph_(),
  selfviewFilter_(nullptr),
  roiInterface_(nullptr),
  videoFormat_(""),
  videoSendIniated_(false),
  audioInputGraph_(),
  audioOutputGraph_(),
  aec_(nullptr),
  mixer_(),
  audioInputInitialized_(false),
  audioOutputInitialized_(false),
  format_()
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

  if (selfViews.size() > 1)
  {
    roiInterface_ = selfViews.at(1);
  }
  else
  {
    Logger::getLogger()->printProgramWarning(this, "RoI surface not set, RoI usage not possible");
  }

  selfviewFilter_ =
      std::shared_ptr<DisplayFilter>(new DisplayFilter("Self", stats_, hwResources_, selfViews, 1111));

  initCameraSelfView();
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
    initCameraSelfView();

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


void FilterGraph::initCameraSelfView()
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
  }

  // create screen share filter, but it is stopped at the beginning
  if (screenShareGraph_.empty())
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
    if (!cameraGraph_.empty())
    {
      addToGraph(std::shared_ptr<Filter>(new LibYUVConverter("", stats_, hwResources_,
                                                             cameraGraph_.at(0)->outputType())), cameraGraph_, 0);
      addToGraph(resizeFilter, cameraGraph_, cameraGraph_.size() - 1);
      addToGraph(selfviewFilter_, cameraGraph_, cameraGraph_.size() - 1);
    }

    if (!screenShareGraph_.empty())
    {
      addToGraph(std::shared_ptr<Filter>(new LibYUVConverter("", stats_, hwResources_,
                                                             screenShareGraph_.at(0)->outputType())),
                 screenShareGraph_, 0);
      addToGraph(resizeFilter, screenShareGraph_, screenShareGraph_.size() - 1);
      addToGraph(selfviewFilter_, screenShareGraph_, screenShareGraph_.size() - 1);
    }

    selfviewFilter_->setHorizontalMirroring(true);
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
    initCameraSelfView();
  }

  // we connect mroi to libyuv conversion filter which makes sure the format is correct
  std::shared_ptr<Filter> mRoi =
      std::shared_ptr<Filter>(new ROIManualFilter("", stats_, hwResources_, roiInterface_));
  addToGraph(mRoi, cameraGraph_, 1);
#ifdef KVAZZUP_HAVE_ONNX_RUNTIME
  auto roi = std::shared_ptr<Filter>(new RoiFilter("", stats_, hwResources_, true, roiInterface_));
  addToGraph(roi, cameraGraph_, cameraGraph_.size() - 1);
#endif

  std::shared_ptr<Filter> kvazaar =
      std::shared_ptr<Filter>(new KvazaarFilter("", stats_, hwResources_));

  addToGraph(kvazaar, cameraGraph_, cameraGraph_.size() - 1);
  addToGraph(kvazaar, screenShareGraph_, 0);

  videoSendIniated_ = true;
}


void FilterGraph::initializeAudioInput(bool opus)
{
  audioCapture_ = std::shared_ptr<AudioCaptureFilter>(new AudioCaptureFilter("", format_, stats_, hwResources_));

  if (audioOutput_)
  {
    QObject::connect(audioOutput_.get(), &AudioOutputFilter::outputtingSound,
                     audioCapture_.get(), &AudioCaptureFilter::mute);
  }


  // Do this before adding participants, otherwise AEC filter wont get attached
  addToGraph(audioCapture_, audioInputGraph_);

  if (aec_ == nullptr)
  {
    aec_ = std::make_shared<SpeexAEC>(format_);
    aec_->init();
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

  audioInputInitialized_ = true;
}


void FilterGraph::initializeAudioOutput(bool opus)
{
  Logger::getLogger()->printNormal(this, "Initializing audio output");

  if (aec_ == nullptr)
  {
    aec_ = std::make_shared<SpeexAEC>(format_);
    aec_->init();
  }

  // Provide echo reference and do AGC once more so conference calls will have
  // good volume levels.
  addToGraph(std::make_shared<DSPFilter>("", stats_, hwResources_, aec_, format_,
                                         true, false, false, false, true, AUDIO_OUTPUT_VOLUME, AUDIO_OUTPUT_GAIN),
             audioOutputGraph_);

  audioOutput_ = std::make_shared<AudioOutputFilter>("", stats_, hwResources_, format_);

  addToGraph(audioOutput_, audioOutputGraph_, (unsigned int)audioOutputGraph_.size() - 1);

  if (audioCapture_)
  {
    QObject::connect(audioOutput_.get(),  &AudioOutputFilter::outputtingSound,
                     audioCapture_.get(), &AudioCaptureFilter::mute);
  }

  audioOutputInitialized_ = true;
}


bool FilterGraph::addToGraph(std::shared_ptr<Filter> filter,
                             GraphSegment &graph,
                             size_t connectIndex)
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

      if(filter->inputType() == DT_YUV420VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding libyuv conversion filter to YUV420");
        addToGraph(std::shared_ptr<Filter>(new LibYUVConverter("", stats_, hwResources_,
                                                               graph.at(connectIndex)->outputType())),
                                           graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == DT_YUV420VIDEO &&
              filter->inputType() == DT_RGB32VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding YUV420 to RGB32 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_, hwResources_)),
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
    Logger::getLogger()->printError(this, "Failed to init filter");
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


void FilterGraph::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                              const MediaID& id)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoFramedSource);

  Logger::getLogger()->printNormal(this, "Adding send video", {"SessionID"}, 
                                   QString::number(sessionID));

  // make sure we are generating video
  if(!videoSendIniated_)
  {
    initVideoSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if (!existingConnection(peers_[sessionID]->sendingStreams, id))
  {
    peers_[sessionID]->sendingStreams.push_back(id);
    peers_[sessionID]->videoSenders.push_back(videoFramedSource);

    cameraGraph_.back()->addOutConnection(videoFramedSource);
    videoFramedSource->start();
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding video sender to Filter Graph");
  }
}


void FilterGraph::receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                   VideoInterface *view, const MediaID &id)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoSink);

  checkParticipant(sessionID);

  if (!existingConnection(peers_[sessionID]->receivingStreams, id))
  {
    peers_[sessionID]->receivingStreams.push_back(id);

    std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
    peers_[sessionID]->videoReceivers.push_back(graph);

    addToGraph(videoSink, *graph);
    addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(sessionID, stats_, hwResources_)), *graph, 0);

    std::shared_ptr<DisplayFilter> displayFilter =
        std::shared_ptr<DisplayFilter>(new DisplayFilter(QString::number(sessionID),
                                                         stats_, hwResources_, {view}, sessionID));

    addToGraph(displayFilter, *graph, 1);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding video receiver to Filter Graph");
  }
}


void FilterGraph::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                              const MediaID& id)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioFramedSource);

  if (!audioInputInitialized_)
  {
    initializeAudioInput(audioFramedSource->inputType() == DT_OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if (!existingConnection(peers_[sessionID]->sendingStreams, id))
  {
    peers_[sessionID]->sendingStreams.push_back(id);

    peers_[sessionID]->audioSenders.push_back(audioFramedSource);

    audioInputGraph_.back()->addOutConnection(audioFramedSource);
    audioFramedSource->start();
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding audio sender to Filter Graph");
  }

  mic(settingEnabled(SettingsKey::micStatus));
}


void FilterGraph::receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                                   const MediaID &id)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioSink);

  if (!audioOutputInitialized_)
  {
    initializeAudioOutput(audioSink->outputType() == DT_OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if (!existingConnection(peers_[sessionID]->receivingStreams, id))
  {
    peers_[sessionID]->receivingStreams.push_back(id);

    std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
    peers_[sessionID]->audioReceivers.push_back(graph);

    addToGraph(audioSink, *graph);

    if (audioSink->outputType() == DT_OPUSAUDIO)
    {
      std::shared_ptr<OpusDecoderFilter> decoder =
          std::shared_ptr<OpusDecoderFilter>(new OpusDecoderFilter(sessionID, format_, stats_, hwResources_));
      addToGraph(decoder, *graph, (unsigned int)graph->size() - 1);
    }

    // mixer helps mix the incoming audio streams into one output stream
    if (mixer_ == nullptr)
    {
      mixer_ = std::make_shared<AudioMixer>();
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
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding audio receiver to Filter Graph");
  }
}


bool FilterGraph::existingConnection(std::vector<MediaID>& connections, MediaID id)
{
  for (auto& connection: connections)
  {
    if (connection == id)
    {
      return true;
    }
  }

  return false;
}

void FilterGraph::uninit()
{
  quitting_ = true;
  removeAllParticipants();

  destroyFilters(cameraGraph_);
  videoSendIniated_ = false;

  destroyFilters(screenShareGraph_);

  destroyFilters(audioInputGraph_);
  destroyFilters(audioOutputGraph_);
  audioInputInitialized_ = false;
  audioOutputInitialized_ = false;
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
  if(screenShareGraph_.size() > 0)
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
  else
  {
    Logger::getLogger()->printProgramError(this, "Screen share graph empty");
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
      destroyFilters(audioInputGraph_);
      destroyFilters(audioOutputGraph_);
      audioOutput_ = nullptr;
      audioCapture_ = nullptr;

      videoSendIniated_ = false;
      audioInputInitialized_ = false;
      audioOutputInitialized_ = false;

      if (!quitting_)
      {
        // since destruction removes even the inputs, we must restore them
        initCameraSelfView();
        selectVideoSource();
        mic(settingEnabled(SettingsKey::micStatus));
      }
    }
  }
}


QAudioFormat FilterGraph::createAudioFormat(uint8_t channels, uint32_t sampleRate)
{
  QAudioFormat format;

  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleFormat(QAudioFormat::Int16);

  return format;
}
