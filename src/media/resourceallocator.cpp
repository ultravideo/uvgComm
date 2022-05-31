#include "resourceallocator.h"

#include "processing/yuvconversions.h"

#include "settingskeys.h"
#include "logger.h"
#include "common.h"

const int MIN_OPUS_BITRATE_BITS = 16000;    // 16 kbit/s
const int MAX_OPUS_BITRATE_BITS = 24000;    // 24 kbit/s
const int MIN_HEVC_BITRATE_BITS = 150000;   // 150 kbit/s
const int MAX_HEVC_BITRATE_BITS = 10000000; // 10 Mbit/s


ResourceAllocator::ResourceAllocator():
  avx2_(is_avx2_available()),
  sse41_(is_sse41_available()),
  manualROI_(false),
  audioStreams_(),
  videoStreams_(),
  bitrateMutex_(),
  videoBitrate_(MAX_HEVC_BITRATE_BITS),
  audioBitrate_(MAX_OPUS_BITRATE_BITS)
{}


void ResourceAllocator::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating automatic resource controller settings");
  manualROI_ = settingEnabled(SettingsKey::manualROIStatus);
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


void ResourceAllocator::addRTCPReport(uint32_t sessionID, DataType type,
                                      int32_t lost, uint32_t jitter)
{
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
    if (!startValueSet)
    {
      bitrate = stream.second->bitrate;
      startValueSet = true;
    }
    else if (stream.second->bitrate < bitrate)
    {
      bitrate = stream.second->bitrate;
    }
  }
}


int ResourceAllocator::getBitrate(DataType type)
{
  int bitrate = 0;

  bitrateMutex_.lock();
  if (type == DT_OPUSAUDIO)
  {
    bitrate = audioBitrate_;
  }
  else if (type == DT_HEVCVIDEO)
  {
    bitrate = videoBitrate_;
  }
  bitrateMutex_.unlock();

  return bitrate;
}


std::shared_ptr<StreamInfo> ResourceAllocator::getStreamInfo(uint32_t sessionID, DataType type)
{
  std::shared_ptr<StreamInfo> pointer = nullptr;
  if (type == DT_OPUSAUDIO)
  {
    if (audioStreams_.find(sessionID) == audioStreams_.end())
    {
      audioStreams_[sessionID] = std::shared_ptr<StreamInfo>(new StreamInfo{0, 0,
                                                                            MAX_OPUS_BITRATE_BITS});
    }

    pointer = audioStreams_[sessionID];
  }
  else if (type == DT_HEVCVIDEO)
  {
    if (videoStreams_.find(sessionID) == videoStreams_.end())
    {
      videoStreams_[sessionID] = std::shared_ptr<StreamInfo>(new StreamInfo{0, 0,
                                                                            MAX_HEVC_BITRATE_BITS});
    }

    pointer = videoStreams_[sessionID];
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Resource allocator tries to adjust unimplemented bit rate");
  }
  return pointer;
}


void ResourceAllocator::limitBitrate(int& bitrate, DataType type)
{
  if (type == DT_OPUSAUDIO)
  {
    if (bitrate < MIN_OPUS_BITRATE_BITS)
    {
      bitrate = MIN_OPUS_BITRATE_BITS;
    }
    else if (bitrate > MAX_OPUS_BITRATE_BITS)
    {
      bitrate = MAX_OPUS_BITRATE_BITS;
    }
  }
  else if (type == DT_HEVCVIDEO)
  {
    if (bitrate < MIN_HEVC_BITRATE_BITS)
    {
      bitrate = MIN_HEVC_BITRATE_BITS;
    }
    else if (bitrate > MAX_HEVC_BITRATE_BITS)
    {
      bitrate = MAX_HEVC_BITRATE_BITS;
    }
  }
}
