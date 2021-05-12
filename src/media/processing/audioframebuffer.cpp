#include "audioframebuffer.h"

AudioFrameBuffer::AudioFrameBuffer(uint32_t desiredFrameSize):
  desiredFrameSize_(desiredFrameSize),
  readyFrames_(),
  partialFrame_(new uint8_t [desiredFrameSize]),
  partialFrameSize_(0)
{}


AudioFrameBuffer::~AudioFrameBuffer()
{
  delete[] partialFrame_;
  partialFrame_ = nullptr;
  partialFrameSize_ = 0;

  readyBuffer_.lock();
  for (unsigned int i = 0; i < readyFrames_.size(); ++i)
  {
    delete [] readyFrames_.at(i);
  }

  readyFrames_.clear();
  readyBuffer_.unlock();
}


void AudioFrameBuffer::inputData(uint8_t* data, uint32_t dataAmount)
{
  if (data == nullptr)
  {
    return;
  }

  uint8_t* totalData = data;
  uint32_t totalDataAmount = dataAmount;

  // Step 1: Take previous partial frame into account if it exists
  if (partialFrameSize_ != 0)
  {
    totalData = new uint8_t [partialFrameSize_ + dataAmount];
    totalDataAmount = partialFrameSize_ + dataAmount;

    memcpy(totalData,                     partialFrame_, partialFrameSize_);
    memcpy(totalData + partialFrameSize_, data,          dataAmount);

    partialFrameSize_ = 0;
  }

  uint32_t processedData = 0;

  // Step 2: Get all ready frames from the partial frame + input data
  while (processedData + desiredFrameSize_ <= totalDataAmount)
  {
    addSampleToBuffer(totalData + processedData, desiredFrameSize_);
    processedData += desiredFrameSize_;
  }

  // Step 3: Save remaining data into partial frame
  if (processedData < totalDataAmount)
  {
    partialFrameSize_ = totalDataAmount - processedData;
    memcpy(partialFrame_, totalData + processedData, partialFrameSize_);
  }

  // totaldata is deleted if necessary
  if (totalDataAmount != dataAmount)
  {
    delete[] totalData;
  }
}


uint8_t *AudioFrameBuffer::readFrame()
{
  uint8_t* outFrame = nullptr;

  readyBuffer_.lock();
  if (!readyFrames_.empty())
  {
    outFrame = readyFrames_.back();
    readyFrames_.pop_back();
  }
  readyBuffer_.unlock();

  return outFrame;
}


void AudioFrameBuffer::changeDesiredFrameSize(uint32_t desiredSize)
{
  readyBuffer_.lock();
  readyFrames_.clear();
  readyBuffer_.unlock();

  desiredFrameSize_ = desiredSize;

  if (partialFrame_ != nullptr)
  {
    delete [] partialFrame_;
    partialFrameSize_ = 0;
  }

  partialFrame_ = new uint8_t [desiredSize];
}


void AudioFrameBuffer::addSampleToBuffer(uint8_t *sample, int sampleSize)
{
  uint8_t* new_sample = new uint8_t[sampleSize];
  memcpy(new_sample, sample, sampleSize);

  readyBuffer_.lock();
  readyFrames_.push_front(new_sample);
  readyBuffer_.unlock();
}
