#include "audiomixer.h"

#include "filter.h"
#include "common.h"

#include <QString>

AudioMixer::AudioMixer():
  inputs_(0),
  mixingMutex_(),
  mixingBuffer_()
{}


std::unique_ptr<uchar[]> AudioMixer::mixAudio(std::unique_ptr<Data> input,
                                              uint32_t sessionID)
{
  std::unique_ptr<uchar[]> outputFrame = nullptr;
  mixingMutex_.lock();
  // mix if there is already a sample for this stream in buffer
  if (mixingBuffer_.find(sessionID) != mixingBuffer_.end())
  {
    printWarning(this, "Mixing overflow. Missing samples.");
    outputFrame = doMixing(input->data_size);
    mixingBuffer_[sessionID] = std::move(input);
  }
  else
  {
    uint32_t size = input->data_size;
    mixingBuffer_[sessionID] = std::move(input);

    // mix if there is a sample for all streams
    if (mixingBuffer_.size() == inputs_)
    {
      outputFrame = doMixing(size);
    }
  }

  mixingMutex_.unlock();

  return outputFrame;
}


std::unique_ptr<uchar[]> AudioMixer::doMixing(uint32_t frameSize)
{
  // don't do mixing if we have only one stream.
  if (mixingBuffer_.size() == 1)
  {
    std::unique_ptr<uchar[]> oneSample =
        std::move(mixingBuffer_.begin()->second->data);
    mixingBuffer_.clear();
    return oneSample;
  }

  std::unique_ptr<uchar[]> result =
      std::unique_ptr<uint8_t[]>(new uint8_t[frameSize]);
  int16_t * output_ptr = (int16_t*)result.get();

  for (unsigned int i = 0; i < frameSize/2; ++i)
  {
    int32_t sum = 0;

    // This is in my understanding the correct way to do audio mixing.
    // Just add them up.
    for (auto& buffer : mixingBuffer_)
    {
      sum += *((int16_t*)buffer.second->data.get() + i);
    }

    // clipping is not desired, but occurs rarely
    // TODO: Replace this with dynamic range compression
    if (sum > INT16_MAX)
    {
      printWarning(this, "Clipping audio", "Value", QString::number(sum));
      sum = INT16_MAX;
    }
    else if (sum < INT16_MIN)
    {
      printWarning(this, "Boosting audio", "Value", QString::number(sum));
      sum = INT16_MIN;
    }

    *output_ptr = sum;
    ++output_ptr;
  }

  mixingBuffer_.clear();

  return result;
}
