#include "audiomixer.h"

#include "common.h"
#include "global.h"
#include "logger.h"
#include "settingskeys.h"

#include <QString>

#include <cmath>

const unsigned int MAX_MIX_BUFFER = AUDIO_FRAMES_PER_SECOND/5;

const int16_t DEFAULT_COMPRESSION_THRESHOLD = INT16_MAX/2;
const uint8_t DEFAULT_COMPRESSION_RATIO = 5;


AudioMixer::AudioMixer():
  inputs_(0),
  mixingMutex_(),
  mixingBuffer_(),
  compressionThreshold_(DEFAULT_COMPRESSION_THRESHOLD),
  compressionRatio_(DEFAULT_COMPRESSION_RATIO)
{}


void AudioMixer::updateSettings()
{
  compressionThreshold_ = dbToint16(settingValue(SettingsKey::audioCompressionThreshold));
  compressionRatio_ = settingValue(SettingsKey::audioCompressionRatio);
}


std::unique_ptr<Data> AudioMixer::mixAudio(std::unique_ptr<Data> input,
                                           std::unique_ptr<Data> potentialOutput,
                                           uint32_t sessionID)
{
  mixingMutex_.lock();

  // make sure the buffer exists for this sessionID
  if (mixingBuffer_.find(sessionID) == mixingBuffer_.end())
  {
    mixingBuffer_[sessionID] = std::deque<std::unique_ptr<Data>>();
  }

  // store size before moving sample to buffer
  uint32_t data_size = input->data_size;

  // add new sample to its buffer
  mixingBuffer_[sessionID].push_back(std::move(input));

  // check if all buffers have a sample we can mix
  unsigned int samples = 0;
  for (auto& buffer : mixingBuffer_)
  {
    if (!buffer.second.empty())
    {
      ++samples;
    }
  }

  // if all inputs have provided a sample for mixing
  if (samples == inputs_)
  {
    potentialOutput->data = doMixing(data_size);
    mixingMutex_.unlock();
    return std::move(potentialOutput);
  }
  else if (mixingBuffer_.at(sessionID).size() >= MAX_MIX_BUFFER)
  {
    Logger::getLogger()->printWarning(this, "Too many samples from one source and not enough from others. "
                                            "Forced mixing to avoid latency",
                                      "Buffer status", QString::number(mixingBuffer_.at(sessionID).size()) + "/" +
                                                         QString::number(MAX_MIX_BUFFER));

    potentialOutput->data = doMixing(data_size);
    mixingMutex_.unlock();
    return std::move(potentialOutput);
  }

  mixingMutex_.unlock();

  return std::unique_ptr<Data> (nullptr);
}


std::unique_ptr<uchar[]> AudioMixer::doMixing(uint32_t frameSize)
{
  // don't do mixing if we have only one stream.
  if (mixingBuffer_.size() == 1)
  {
    if (!mixingBuffer_.begin()->second.empty())
    {
    std::unique_ptr<uchar[]> oneSample =
        std::move(mixingBuffer_.begin()->second.front()->data);
    mixingBuffer_.begin()->second.pop_front();
    mixingBuffer_.clear();
    return oneSample;
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Tried to mix without anything to mix");
      return nullptr;
    }
  }

  std::unique_ptr<uchar[]> result =
      std::unique_ptr<uint8_t[]>(new uint8_t[frameSize]);
  int16_t * output_ptr = (int16_t*)result.get();

  for (unsigned int i = 0; i < frameSize/2; ++i)
  {
    int32_t sum = 0;

    // This is in my understanding the correct way to do audio mixing.
    // Just add them up and divide by number of participants
    // the level should be corrected with Speex AGC
    for (auto& buffer : mixingBuffer_)
    {
      if (!buffer.second.empty())
      {
        sum += *((int16_t*)buffer.second.front()->data.get() + i);
      }
    }

    // audio compression, reduces the peaks in audio
    if (sum > compressionThreshold_)
    {
      Logger::getLogger()->printWarning(this, "Compressing audio");
      sum = compressionThreshold_ + (sum - compressionThreshold_)/compressionRatio_;
    }
    else if (sum < -compressionThreshold_)
    {
      Logger::getLogger()->printWarning(this, "Compressing audio");
      sum = -compressionThreshold_ + (sum + compressionThreshold_)/compressionRatio_;
    }

    // limiting
    if (sum > INT16_MAX)
    {
      Logger::getLogger()->printWarning(this, "Limiting mixed audio to maximum");
      sum = INT16_MAX;
    }
    else if (sum < INT16_MIN)
    {
      Logger::getLogger()->printWarning(this, "Limiting mixed audio to minimum");
      sum = INT16_MIN;
    }

    *output_ptr = (int16_t)sum;
    ++output_ptr;
  }

  // remove the samples that were mixed
  for (auto& buffer : mixingBuffer_)
  {
    if (!buffer.second.empty())
    {
      buffer.second.pop_front();
    }
  }

  return result;
}


int16_t AudioMixer::dbToint16(float dB) const
{
  if (dB == 0)
  {
    return INT16_MAX;
  }

  float amplitude = std::pow(10, dB/20.0f);
  float value = amplitude*INT16_MAX;

  return static_cast<int16_t>(std::clamp(value, float(INT16_MIN), float(INT16_MAX)));
}


float AudioMixer::int16ToDB(int16_t value) const
{
  float normalized = float(value)/float(INT_MAX);

  if (normalized == 0.0f)
  {
    return -96.0f; // really quiet
  }

  float magnitude = std::fabs(normalized);

  // log of less than one is negative, as should be. DB is always negative or 0 dB
  return 20.0f * std::log10(magnitude);
}
