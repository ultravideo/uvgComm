#include "filtergraphp2p.h"

#include "media/processing/camerafilter.h"
#include "media/processing/screensharefilter.h"
#include "media/processing/kvazaarfilter.h"
#include "media/processing/roimanualfilter.h"
#include "media/processing/hybridfilter.h"

#include "media/processing/openhevcfilter.h"


#include "media/processing/halfrgbfilter.h"
#include "media/processing/libyuvconverter.h"

#include "media/processing/displayfilter.h"
#include "media/processing/audiocapturefilter.h"
#include "media/processing/opusencoderfilter.h"
#include "media/processing/opusdecoderfilter.h"
#include "media/processing/dspfilter.h"
#include "media/processing/audiomixerfilter.h"
#include "media/processing/audiooutputfilter.h"

#include "media/delivery/uvgrtpsender.h"

#ifdef uvgComm_HAVE_ONNX_RUNTIME
#include "media/processing/roiyolofilter.h"
#endif

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

FilterGraphP2P::FilterGraphP2P()
  : FilterGraph(),

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


FilterGraphP2P::~FilterGraphP2P()
{}


void FilterGraphP2P::setSelfViews(QList<VideoInterface*> selfViews)
{
  if (selfViews.size() > 1)
  {
    roiInterface_ = selfViews.at(0);
  }
  else
  {
    Logger::getLogger()->printProgramWarning(this, "RoI surface not set, RoI usage not possible");
  }

  selfviewFilter_ =
      std::shared_ptr<DisplayFilter>(new DisplayFilter("Self", stats_, hwResources_, selfViews, 1111));

  initCameraSelfView();
}


void FilterGraphP2P::setConferenceSize(uint32_t otherParticipants)
{
  if (kvazaar_)
  {
    kvazaar_->setConferenceSize(otherParticipants);
  }

  if (libyuv_)
  {
    libyuv_->setConferenceSize(otherParticipants);
  }
}


void FilterGraphP2P::updateConferenceSize()
{
  uint32_t otherParticipants = 0;

  for (auto& peer : peers_)
  {
    if (peer.second != nullptr)
    {
      otherParticipants += peer.second->videoReceivers.size();
    }
  }

  setConferenceSize(otherParticipants);
}


void FilterGraphP2P::uninit()
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


void FilterGraphP2P::updateVideoSettings()
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
            cameraGraph_.back()->addOutConnection(senderFilter.second);
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

  if (mixer_)
  {
    mixer_->updateSettings();
  }

  selectVideoSource();

  FilterGraph::updateVideoSettings();
}


void FilterGraphP2P::updateAudioSettings()
{
  for (auto& filter : audioInputGraph_)
  {
    filter->updateSettings();
  }

  for (auto& filter : audioOutputGraph_)
  {
    filter->updateSettings();
  }

  if (aec_)
  {
    aec_->updateSettings();
  }

  mic(settingEnabled(SettingsKey::micStatus));

  FilterGraph::updateAudioSettings();
}


void FilterGraphP2P::initCameraSelfView()
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

    std::shared_ptr<Filter> resizeFilter1 = std::shared_ptr<Filter>(new HalfRGBFilter("", stats_, hwResources_));
    std::shared_ptr<Filter> resizeFilter2 = std::shared_ptr<Filter>(new HalfRGBFilter("", stats_, hwResources_));
    if (!cameraGraph_.empty())
    {
      libyuv_ = std::shared_ptr<LibYUVConverter>(new LibYUVConverter("", stats_, hwResources_,
                                                            cameraGraph_.at(0)->outputType()));
      addToGraph(libyuv_, cameraGraph_, 0);
      addToGraph(resizeFilter1, cameraGraph_, cameraGraph_.size() - 1);
      addToGraph(selfviewFilter_, cameraGraph_, cameraGraph_.size() - 1);
    }

    if (!screenShareGraph_.empty())
    {
      addToGraph(std::shared_ptr<LibYUVConverter>(new LibYUVConverter("", stats_, hwResources_,
                                                             screenShareGraph_.at(0)->outputType())),
                 screenShareGraph_, 0);
      addToGraph(resizeFilter2, screenShareGraph_, screenShareGraph_.size() - 1);
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


void FilterGraphP2P::initVideoSend()
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
#ifdef uvgComm_HAVE_ONNX_RUNTIME
  auto roi = std::shared_ptr<Filter>(new ROIYoloFilter("", stats_, hwResources_, true, roiInterface_));
  addToGraph(roi, cameraGraph_, cameraGraph_.size() - 1);
#endif

  kvazaar_ = std::shared_ptr<KvazaarFilter>(new KvazaarFilter("", stats_, hwResources_));

  addToGraph(kvazaar_, cameraGraph_, cameraGraph_.size() - 1);
  addToGraph(kvazaar_, screenShareGraph_, 0);

  hybrid_ = std::shared_ptr<HybridFilter>(new HybridFilter("", stats_, hwResources_));
  addToGraph(hybrid_, cameraGraph_, cameraGraph_.size() - 1);
  //addToGraph(hybrid_, screenShareGraph_, screenShareGraph_.size() - 1);

  videoSendIniated_ = true;
}


void FilterGraphP2P::initializeAudioInput(bool opus)
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
                                               false, true, true, true, true,
                                               AUDIO_INPUT_VOLUME, AUDIO_INPUT_GAIN));

  addToGraph(dspProcessor, audioInputGraph_, (unsigned int)audioInputGraph_.size() - 1);

  if (opus)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusEncoderFilter("", format_, stats_, hwResources_)),
               audioInputGraph_, (unsigned int)audioInputGraph_.size() - 1);
  }

  audioInputInitialized_ = true;
}


void FilterGraphP2P::initializeAudioOutput(bool opus)
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
                                         true, false, false, false, true,
                                         AUDIO_OUTPUT_VOLUME, AUDIO_OUTPUT_GAIN),
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

void FilterGraphP2P::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> sender,
                                 uint32_t localSSRC, uint32_t remoteSSRC,
                                 const QString& remoteCNAME, bool isP2P)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(sender);

  Logger::getLogger()->printNormal(this, "Adding send video", {"SessionID"},
                                   QString::number(sessionID));

  // make sure we are generating video
  if(!videoSendIniated_)
  {
    initVideoSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if (peers_[sessionID]->videoSenders.find(localSSRC) == peers_[sessionID]->videoSenders.end())
  {
    std::shared_ptr<UvgRTPSender> rtpSender = std::dynamic_pointer_cast<UvgRTPSender>(sender);
    peers_[sessionID]->videoSenders[localSSRC] = sender;

    cameraGraph_.back()->addOutConnection(sender);
    sender->start();
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding video sender to Filter Graph",
                                     "Local SSRC", QString::number(localSSRC));
  }
}

void FilterGraphP2P::receiveVideoFrom(uint32_t sessionID,
                                      std::shared_ptr<Filter> receiver,
                                      VideoInterface* view,
                                      uint32_t remoteSSRC)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(receiver);

  checkParticipant(sessionID);

  if (peers_[sessionID]->videoReceivers.find(remoteSSRC) == peers_[sessionID]->videoReceivers.end())
  {
    std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
    peers_[sessionID]->videoReceivers[remoteSSRC] = graph;

    addToGraph(receiver, *graph);
    addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(sessionID, stats_, hwResources_)), *graph, 0);

    std::shared_ptr<DisplayFilter> displayFilter =
        std::shared_ptr<DisplayFilter>(new DisplayFilter(QString::number(sessionID),
                                                         stats_, hwResources_, {view}, sessionID));

    addToGraph(displayFilter, *graph, 1);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding video receiver to Filter Graph",
                                     "Remote SSRC", QString::number(remoteSSRC));
  }
}


void FilterGraphP2P::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> sender,
                                 uint32_t localSSRC)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(sender);

  if (!audioInputInitialized_)
  {
    initializeAudioInput(sender->inputType() == DT_OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if ((peers_[sessionID]->audioSenders.find(localSSRC) == peers_[sessionID]->audioSenders.end()))
  {
    peers_[sessionID]->audioSenders[localSSRC] = sender;

    audioInputGraph_.back()->addOutConnection(sender);
    sender->start();
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Found existing filter, not adding audio sender to Filter Graph");
  }

  mic(settingEnabled(SettingsKey::micStatus));
}

void FilterGraphP2P::
    receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver, uint32_t remoteSSRC)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(receiver);

  if (!audioOutputInitialized_)
  {
    initializeAudioOutput(receiver->outputType() == DT_OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);

  if (peers_[sessionID]->audioReceivers.find(remoteSSRC) == peers_[sessionID]->audioReceivers.end())
  {
    std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
    peers_[sessionID]->audioReceivers[remoteSSRC] = graph;

    addToGraph(receiver, *graph);

    if (receiver->outputType() == DT_OPUSAUDIO)
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


void FilterGraphP2P::mic(bool state)
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


void FilterGraphP2P::camera(bool state)
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


void FilterGraphP2P::screenShare(bool shareState)
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


void FilterGraphP2P::selectVideoSource()
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


void FilterGraphP2P::running(bool state)
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

  FilterGraph::running(state);
}


void FilterGraphP2P::destroyPeer(Peer* peer)
{
  Logger::getLogger()->printNormal(this, "Destroying peer from P2P Filter Graph");

  for (auto& audioSender : peer->audioSenders)
  {
    audioInputGraph_.back()->removeOutConnection(audioSender.second);
  }
  for (auto& videoSender : peer->videoSenders)
  {
    cameraGraph_.back()->removeOutConnection(videoSender.second);
  }

  FilterGraph::destroyPeer(peer);
}


void FilterGraphP2P::lastPeerRemoved()
{
  destroyFilters(cameraGraph_);
  destroyFilters(screenShareGraph_);
  destroyFilters(audioInputGraph_);
  destroyFilters(audioOutputGraph_);
  audioOutput_ = nullptr;
  audioCapture_ = nullptr;

  libyuv_ = nullptr;
  kvazaar_ = nullptr;
  hybrid_ = nullptr;

  videoSendIniated_ = false;
  audioInputInitialized_ = false;
  audioOutputInitialized_ = false;

  if (!quitting_)
  {
    // since destruction removes even the inputs, we must restore them
    initCameraSelfView();
    mic(settingEnabled(SettingsKey::micStatus));
  }
}


QAudioFormat FilterGraphP2P::createAudioFormat(uint8_t channels, uint32_t sampleRate)
{
  QAudioFormat format;

  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleFormat(QAudioFormat::Int16);

  return format;
}
