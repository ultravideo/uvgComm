#include "resourceallocator.h"

#include "global.h"
#include "processing/yuvconversions.h"

#include "settingskeys.h"
#include "logger.h"
#include "common.h"

#include <algorithm>


const int DEFAULT_OPUS_BITRATE_BITS = 24000; // 24 kbps


ResourceAllocator::ResourceAllocator():
  avx2_(is_avx2_available()),
  sse41_(is_sse41_available()),
  manualROI_(false),
  audioStreams_(),
  videoStreams_(),
  bitrateMutex_(),
  conferenceVideoBandwidthBps_(0),
  conferenceAudioBandwidthBps_(DEFAULT_OPUS_BITRATE_BITS),
  roiQp_(0),
  backgroundQp_(0),
  roiObject_(0),
  otherParticipants_(1),
  visibleParticipants_(9),
  bitrateMode_(SINGLE_UPLINK_BITRATE),
  isSpeaker_(false),
  uploadBandwidth_(0),
  hybridPrioritization_(100)
{
  updateSettings();
}


void ResourceAllocator::setArchitectureBitrate(ArchitectureBitrate bitrate)
{
  bitrateMode_ = bitrate;
}


void ResourceAllocator::setParticipants(int otherParticipants)
{
  if (otherParticipants < 1)
  {
    Logger::getLogger()->printProgramWarning(this, "setParticipants called with less than 1 participant, using 1");
    otherParticipants = 1;
  }

  bool changed = false;
  bitrateMutex_.lock();
  otherParticipants_ = otherParticipants;
  bitrateMutex_.unlock();
  
  emit participantsChanged(otherParticipants);

  int actualParticipants = std::min(otherParticipants, visibleParticipants_);
  if (conferenceViewMode_ == "Gallery")
  {
    videoResolution_ = galleryResolution(conferenceResolution_, actualParticipants);
  }
  else if (conferenceViewMode_ == "Speaker")
  {
    if (isSpeaker_)
    {
      videoResolution_ = speakerResolution(conferenceResolution_, actualParticipants);
    }
    else
    {
      videoResolution_ = listenerResolution(conferenceResolution_, actualParticipants);
    }
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Unknown conference mode: " + conferenceViewMode_);
  }
}


void ResourceAllocator::setConferenceResolution(const QSize& resolution)
{
  conferenceResolution_ = resolution;
}


QSize ResourceAllocator::getVideoResolution() const
{
  return videoResolution_;
}


void ResourceAllocator::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating automatic resource controller settings");
  manualROI_ = settingString(SettingsKey::roiMode) == "manual";
  autoROI_ = settingString(SettingsKey::roiMode) == "auto";
  roiQp_ = settingValue(SettingsKey::roiQp);
  backgroundQp_ = settingValue(SettingsKey::backgroundQp);
  roiObject_ = settingValue(SettingsKey::roiObject);
  conferenceViewMode_ = settingString(SettingsKey::sipConferenceMode);
  isSpeaker_ = settingBool(SettingsKey::sipSpeakerMode);
  uploadBandwidth_ = settingValue(SettingsKey::sipUpBandwidth);
  visibleParticipants_ = settingValue(SettingsKey::sipVisibleParticipants);

  // Read and cache framerate settings once during settings update
  framerateNumerator_ = static_cast<uint32_t>(settingValue(SettingsKey::videoFramerateNumerator));
  framerateDenominator_ = static_cast<uint32_t>(settingValue(SettingsKey::videoFramerateDenominator));

  if (settingEnabled(SettingsKey::videoFileEnabled))
  {
    conferenceResolution_ = QSize(settingValue(SettingsKey::videoFileResolutionWidth),
                                  settingValue(SettingsKey::videoFileResolutionHeight));
  }
  else
  {
    conferenceResolution_ = QSize(settingValue(SettingsKey::videoResolutionWidth),
                                  settingValue(SettingsKey::videoResolutionHeight));
  }
  videoResolution_ = conferenceResolution_;

  hybridPrioritization_ = settingValue(SettingsKey::sipHybridPriorization);
}


bool ResourceAllocator::isAVX2Enabled()
{
  return avx2_;
}


bool ResourceAllocator::isSSE41Enabled()
{
  return sse41_;
}


bool ResourceAllocator::useManualROI()
{
  return manualROI_;
}

bool ResourceAllocator::useAutoROI()
{
  return autoROI_;
}

uint16_t ResourceAllocator::getRoiObject() const
{
  return roiObject_;
}


void ResourceAllocator::addRTCPReport(uint32_t sessionID, DataType type,
                                      int32_t lost, uint32_t jitter)
{
  /*
  std::shared_ptr<StreamInfo> info = getStreamInfo(sessionID, type);

  if (jitter > info->previousJitter || lost > info->previousLost)
  {
    if (lost > info->previousLost)
    {
      info->bitrate /= 2;
    }
    else
    {
      info->bitrate *= 0.9;
    }
  }
  else
  {
    info->bitrate *= 1.1;
  }

  limitBitrate(info->bitrate, type);

  info->previousJitter = jitter;
  info->previousLost = lost;

  bitrateMutex_.lock();
  if (type == DT_OPUSAUDIO)
  {
    updateGlobalBitrate(audioBitrate_, audioStreams_);
  }
  else
  {
    updateGlobalBitrate(videoBitrate_, videoStreams_);
  }
  bitrateMutex_.unlock();
*/
}


void ResourceAllocator::updateGlobalBitrate(int& bitrate,
                                            std::map<uint32_t, std::shared_ptr<StreamInfo>>& streams)
{
  if (streams.empty())
  {
    return;
  }

  bool startValueSet = false;

  for (auto& stream : streams)
  {
    if (!startValueSet && stream.second != nullptr)
    {
      bitrate = stream.second->bitrate;
      startValueSet = true;
    }
    else if (stream.second != nullptr && stream.second->bitrate < bitrate)
    {
      bitrate = stream.second->bitrate;
    }
  }
}


void ResourceAllocator::setConferenceBandwidth(DataType type, int bandwidthKbps)
{
  // SDP bandwidth (b=) is in kbps. Convert once here and keep internal storage in bps.
  const int bandwidthBps = std::max(0, bandwidthKbps) * 1000;
  bitrateMutex_.lock();
  if (type == DT_OPUSAUDIO)
  {
    conferenceAudioBandwidthBps_ = bandwidthBps;
  }
  else if (type == DT_HEVCVIDEO)
  {
    conferenceVideoBandwidthBps_ = bandwidthBps;
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented bit rate");
  }
  bitrateMutex_.unlock();
}


std::shared_ptr<StreamInfo> ResourceAllocator::getStreamInfo(uint32_t sessionID, DataType type)
{
  std::shared_ptr<StreamInfo> pointer = nullptr;
  bitrateMutex_.lock();
  if (type == DT_OPUSAUDIO)
  {
    if (audioStreams_.find(sessionID) == audioStreams_.end())
    {
      audioStreams_[sessionID] = std::shared_ptr<StreamInfo>(new StreamInfo{0, 0, 0});
    }

    pointer = audioStreams_[sessionID];
  }
  else if (type == DT_HEVCVIDEO)
  {
    if (videoStreams_.find(sessionID) == videoStreams_.end())
    {
      videoStreams_[sessionID] = std::shared_ptr<StreamInfo>(new StreamInfo{0, 0, 0});
    }

    pointer = videoStreams_[sessionID];
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented bit rate");
  }
  bitrateMutex_.unlock();
  return pointer;
}


int ResourceAllocator::getEncoderBitrate(DataType type)
{
  bitrateMutex_.lock();
  // Conference limits arrive from SDP as bandwidth and are assumed to be on-the-wire totals
  // (including RTP/RTCP overhead). The encoder, however, needs a payload bitrate target.
  int conferenceTotalBandwidthBps = conferenceBandwidthPortion(type);
  int streamBitrateBps = limitUploadBitrate(conferenceTotalBandwidthBps, type);
  bitrateMutex_.unlock();

  Logger::getLogger()->printNormal(this, "Calculated encoder bitrate",
                                   {"Type", "Conference total bw", "Stream payload"},
                                   {datatypeToString(type),
                                    QString::number(conferenceTotalBandwidthBps),
                                    QString::number(streamBitrateBps)});

  return streamBitrateBps;
}


int ResourceAllocator::conferenceBandwidthPortion(DataType type)
{
  int bitrateBps = 0;
  int actualParticipants = std::min(otherParticipants_, visibleParticipants_);
  
  if (type == DT_OPUSAUDIO)
  {
    bitrateBps = conferenceAudioBandwidthBps_;
  }
  else if (type == DT_HEVCVIDEO)
  {
    bitrateBps = conferenceVideoBandwidthBps_;

    if (conferenceViewMode_ == "Gallery")
    {
      bitrateBps = galleryBitrate(conferenceResolution_, bitrateBps, actualParticipants);
    }
    else if (conferenceViewMode_ == "Speaker")
    {
      if (isSpeaker_)
      {
        bitrateBps = speakerBitrate(conferenceResolution_, bitrateBps, actualParticipants);
      }
      else
      {
        bitrateBps = listenerBitrate(conferenceResolution_, bitrateBps, actualParticipants);
      }
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Unknown conference mode: " + conferenceViewMode_);
      bitrateBps = conferenceVideoBandwidthBps_;
    }
  }

  return bitrateBps;
}


int ResourceAllocator::limitUploadBitrate(int bandwidthTargetbps, DataType type)
{
  int qualityTarget = bandwidthTargetbps*0.9;
  int uploadLimit = uploadBandwidth_*0.9;

  if (type == DT_OPUSAUDIO)
  {
    return DEFAULT_OPUS_BITRATE_BITS;
  }
  else if (type == DT_HEVCVIDEO)
  {
    double soloStreamOverhead = calculateVideoOverheadPercentage(uploadLimit, framerateNumerator_, framerateDenominator_, IPV6_OVERHEAD, DEFAULT_MTU_BYTES);
    double soloTargetLimit = uploadBandwidth_ * (1.0 - soloStreamOverhead);
    double multiStreamEstimate = (uploadBandwidth_ / otherParticipants_)*0.9; // quick estimate for stream limit
    double multiStreamOverhead = calculateVideoOverheadPercentage(multiStreamEstimate, framerateNumerator_, framerateDenominator_, IPV6_OVERHEAD, DEFAULT_MTU_BYTES);
    double multiTargetLimit = (uploadBandwidth_ / otherParticipants_) * (1.0 - multiStreamOverhead);

    if (bitrateMode_ == SINGLE_UPLINK_BITRATE)
    {
      uploadLimit = soloTargetLimit;
    }
    else if (bitrateMode_ == MULTI_UPLINK_BITRATE)
    {
      uploadLimit = multiTargetLimit;
    }
    else if (bitrateMode_ == HYBRID_UPLINK_BITRATE)
    {
      uploadLimit = (soloTargetLimit - multiTargetLimit) * ((double)hybridPrioritization_/100) + multiTargetLimit;
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Unknown bitrate mode: " + QString::number(bitrateMode_));
    }
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented stream type limits");
  }

  return std::min(qualityTarget, uploadLimit) - DEFAULT_OPUS_BITRATE_BITS;
}


uint8_t ResourceAllocator::getRoiQp() const
{
  return roiQp_;
}


uint8_t ResourceAllocator::getBackgroundQp() const
{
  return backgroundQp_;
}


int ResourceAllocator::getStreamBandwidthUsage(DataType type)
{
  bitrateMutex_.lock();

  const int conferenceTotalBandwidthBps = conferenceBandwidthPortion(type);
  int streamTotalBandwidthBps = 0;

  if (type == DT_OPUSAUDIO)
  {
    streamTotalBandwidthBps = DEFAULT_OPUS_BITRATE_BITS;
  }
  else if (type == DT_HEVCVIDEO)
  {
    const int qualityTarget = conferenceTotalBandwidthBps;
    const int soloTargetLimit = uploadBandwidth_;
    const int multiTargetLimit = uploadBandwidth_ / otherParticipants_;

    int uploadLimit = soloTargetLimit;
    if (bitrateMode_ == SINGLE_UPLINK_BITRATE)
    {
      uploadLimit = soloTargetLimit;
    }
    else if (bitrateMode_ == MULTI_UPLINK_BITRATE)
    {
      uploadLimit = multiTargetLimit;
    }
    else if (bitrateMode_ == HYBRID_UPLINK_BITRATE)
    {
      uploadLimit = static_cast<int>((soloTargetLimit - multiTargetLimit) * (static_cast<double>(hybridPrioritization_) / 100.0)
                                     + multiTargetLimit);
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Unknown bitrate mode: " + QString::number(bitrateMode_));
    }

    streamTotalBandwidthBps = std::min(qualityTarget, uploadLimit);
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented bit rate");
    streamTotalBandwidthBps = conferenceTotalBandwidthBps;
  }

  bitrateMutex_.unlock();

  return std::max(0, streamTotalBandwidthBps);
}
