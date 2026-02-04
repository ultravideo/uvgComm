#include "resourceallocator.h"

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
  conferenceVideoBitrate_(0),
  conferenceAudioBitrate_(DEFAULT_OPUS_BITRATE_BITS),
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
  changed = (otherParticipants_ != otherParticipants);
  otherParticipants_ = otherParticipants;
  bitrateMutex_.unlock();

  if (changed)
  {
    emit participantsChanged(otherParticipants);
  }

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


void ResourceAllocator::setConferenceBitrate(DataType type, int bitrate)
{
  bitrateMutex_.lock();
  if (type == DT_OPUSAUDIO)
  {
    conferenceAudioBitrate_ = bitrate;
  }
  else if (type == DT_HEVCVIDEO)
  {
    conferenceVideoBitrate_ = bitrate;
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented bit rate");
  }
  bitrateMutex_.unlock();
}


int ResourceAllocator::getEncoderBitrate(DataType type)
{
  bitrateMutex_.lock();
  int bitrate = conferenceBitratePortion(type);
  int limitedBitrate = limitUploadBitrate(bitrate, type);
  bitrateMutex_.unlock();

  Logger::getLogger()->printNormal(this, "Calculated conference bitrate portion",
                                {"Type", "Bitrate", "Limited Bitrate"},
                                {datatypeToString(type), QString::number(bitrate), QString::number(limitedBitrate)});

  return limitedBitrate;
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


int ResourceAllocator::conferenceBitratePortion(DataType type)
{
  int bitrate = 0;
  int actualParticipants = std::min(otherParticipants_, visibleParticipants_);
  
  if (type == DT_OPUSAUDIO)
  {
    bitrate = conferenceAudioBitrate_;
  }
  else if (type == DT_HEVCVIDEO)
  {
    bitrate = conferenceVideoBitrate_;

    if (conferenceViewMode_ == "Gallery")
    {
      bitrate = galleryBitrate(conferenceResolution_, bitrate, actualParticipants);
    }
    else if (conferenceViewMode_ == "Speaker")
    {
      if (isSpeaker_)
      {
        bitrate = speakerBitrate(conferenceResolution_, bitrate, actualParticipants);
      }
      else
      {
        bitrate = listenerBitrate(conferenceResolution_, bitrate, actualParticipants);
      }
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Unknown conference mode: " + conferenceViewMode_);
      bitrate = conferenceVideoBitrate_;
    }
  }

  return bitrate;
}

int ResourceAllocator::limitUploadBitrate(int bitrate, DataType type)
{
  if (type == DT_OPUSAUDIO)
  {
    bitrate = DEFAULT_OPUS_BITRATE_BITS;
  }
  else if (type == DT_HEVCVIDEO)
  {
    int maxOverallBandwidth = uploadBandwidth_ * (1.0 - TRANSMISSION_OVERHEAD); // leave room for overhead
    int maxVideoBitrate = maxOverallBandwidth - DEFAULT_OPUS_BITRATE_BITS; // reduce audio the get video

    if (bitrateMode_ == SINGLE_UPLINK_BITRATE)
    {
      // limit bandwidth if we cannot send even one copy of our selected bitrate
      bitrate = std::min(bitrate, maxVideoBitrate);
    }
    else if (bitrateMode_ == MULTI_UPLINK_BITRATE)
    {
      // limit bandwidth if we cannot send the selected bit rate to all sending targets
      bitrate = std::min(bitrate, maxVideoBitrate / otherParticipants_);
    }
    else if (bitrateMode_ == HYBRID_UPLINK_BITRATE)
    {
      int participants = std::max(1, otherParticipants_); // use actual send count, not visible-only
      int perParticipant = maxVideoBitrate / participants;
      int weightedBitrate = (maxVideoBitrate - perParticipant) * (hybridPrioritization_/100) +
                            perParticipant;
      bitrate = std::min(bitrate, weightedBitrate);
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Unknown bitrate mode: " + QString::number(bitrateMode_));
      bitrate = uploadBandwidth_; // default fallback
    }
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented bit rate");
  }

  return bitrate;
}


uint8_t ResourceAllocator::getRoiQp() const
{
  return roiQp_;
}

uint8_t ResourceAllocator::getBackgroundQp() const
{
  return backgroundQp_;
}
