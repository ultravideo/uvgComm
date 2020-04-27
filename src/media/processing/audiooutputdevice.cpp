#include "audiooutputdevice.h"

#include "filter.h"
#include "statisticsinterface.h"

#include "common.h"
#include "global.h"

#include <QDateTime>

#include <QDebug>

AudioOutputDevice::AudioOutputDevice(StatisticsInterface *stats):
  QIODevice(),
  stats_(stats),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  audioOutput_(nullptr),
  output_(nullptr),
  format_(),
  sampleMutex_(),
  outputSample_(),
  sampleSize_(0),
  inputs_(0)
{}


AudioOutputDevice::~AudioOutputDevice()
{}


void AudioOutputDevice::initializeAudio(QAudioFormat format)
{
  QAudioDeviceInfo info(device_);
  if (!info.isFormatSupported(format)) {
    printDebug(DEBUG_WARNING, this, "Default format not supported - trying to use nearest.");
    format_ = info.nearestFormat(format);
  }
  else
  {
    format_ = format;
  }

  createAudioOutput();
}


void AudioOutputDevice::createAudioOutput()
{
  if(audioOutput_)
  {
    delete audioOutput_;
  }
  audioOutput_ = new QAudioOutput(device_, format_, this);

  resetBuffer();

  open(QIODevice::ReadOnly);
  // pull mode
  audioOutput_->start(this);
}

void AudioOutputDevice::resetBuffer()
{
  sampleSize_ = format_.sampleRate()*format_.bytesPerFrame()/AUDIO_FRAMES_PER_SECOND;
  outputSample_ = std::unique_ptr<uint8_t[]>(new uint8_t[sampleSize_]);
  memset(outputSample_.get(), 0, sampleSize_);
}


void AudioOutputDevice::deviceChanged(int index)
{
  Q_UNUSED(index);
  printUnimplemented(this, "Audio output device change not implemented fully.");

  audioOutput_->stop();
  audioOutput_->disconnect(this);
  //initializeAudio(format_);
}


void AudioOutputDevice::volumeChanged(int value)
{
  if (audioOutput_)
    audioOutput_->setVolume(qreal(value/100.0f));
}


qint64 AudioOutputDevice::readData(char *data, qint64 maxlen)
{
  uint32_t read = 0;
  if (maxlen < sampleSize_)
  {
    printWarning(this, "Read too little audio data.", {"Read vs available"}, {
                   QString::number(maxlen) + " vs " + QString::number(sampleSize_)});
  }
  else
  {
    // if no new input has arrived, we play the last sample
    sampleMutex_.lock();
    memcpy(data, outputSample_.get(), sampleSize_);
    read = sampleSize_;
    sampleMutex_.unlock();
  }

  return read;
}


qint64 AudioOutputDevice::writeData(const char *data, qint64 len)
{
  Q_UNUSED(data);
  Q_UNUSED(len);

  // output does not write data to device, only reads it.
  printProgramWarning(this, "Why is output writing to us");

  return 0;
}

qint64 AudioOutputDevice::bytesAvailable() const
{
  printNormal(this, "bytes avaialbe");
  return sampleSize_ + QIODevice::bytesAvailable();
}


void AudioOutputDevice::takeInput(std::unique_ptr<Data> input, uint32_t sessionID)
{
  if (audioOutput_ && audioOutput_->state() != QAudio::StoppedState)
  {
    // Add audio delay to statistics
    int64_t delay = QDateTime::currentMSecsSinceEpoch() -
        ((uint64_t)input->presentationTime.tv_sec * 1000 +
         (uint64_t)input->presentationTime.tv_usec/1000);

    stats_->receiveDelay(sessionID, "Audio", delay);

    int dataLeft = input->data_size;

    // we record one sample to buffer in case there is a packet loss,
    // just so we don't have any breaks
    std::unique_ptr<uchar[]> outputFrame;

    if (inputs_ < 2)
    {
      outputFrame = std::move(input->data);
    }
    else
    {
      outputFrame = mixAudio(std::move(input), sessionID);

      // if we are still waiting for other samples to mix
      if (outputFrame == nullptr)
      {
        return;
      }
    }

    sampleMutex_.lock();
    sampleSize_ = dataLeft;
    outputSample_ = std::move(outputFrame);
    sampleMutex_.unlock();
/*
    int audioChunks = audioOutput_->bytesFree()/audioOutput_->periodSize();

    const char *pointer = (char *)outputFrame.get();
    while (audioChunks > 0 && dataLeft >= audioOutput_->periodSize())
    {
      qint64 written = output_->write(pointer, audioOutput_->periodSize());

      if (written == -1)
      {
        printError(this, "Audio output error");
        break;
      }
      dataLeft -= written;
      pointer += written;

      --audioChunks;
    }

    if (dataLeft >= audioOutput_->periodSize())
    {
      printWarning(this, "Audio output data overflow. "
                         "Output device cannot play this much audio. Discarding audio");
    }
    else if (dataLeft > 0 && audioChunks > 0)
    {
      printError(this, "Audio framesize is wrong. Discarding some part of it."
                       "It is recommended to use the same frame size as output period. "
                       "This could be fixed by recording the leftovers also.");
    }
    */
  }
}


std::unique_ptr<uchar[]> AudioOutputDevice::mixAudio(std::unique_ptr<Data> input, uint32_t sessionID)
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


std::unique_ptr<uchar[]> AudioOutputDevice::doMixing(uint32_t frameSize)
{
  // don't do mixing if we have only one stream.
  if (mixingBuffer_.size() == 1)
  {
    std::unique_ptr<uchar[]> oneSample = std::move(mixingBuffer_.begin()->second->data);
    mixingBuffer_.clear();
    return oneSample;
  }

  std::unique_ptr<uchar[]> result = std::unique_ptr<uint8_t[]>(new uint8_t[frameSize]);
  int16_t * output_ptr = (int16_t*)result.get();

  for (unsigned int i = 0; i < frameSize/2; ++i)
  {
    int32_t sum = 0;

    // This is in my understanding the correct way to do audio mixing. Just add them up.
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


void AudioOutputDevice::start()
{
  open(QIODevice::ReadOnly);

  if(audioOutput_->state() == QAudio::SuspendedState
     || audioOutput_->state() == QAudio::StoppedState)
  {
    audioOutput_->resume();
  }
}


void AudioOutputDevice::stop()
{
  if(audioOutput_->state() == QAudio::ActiveState)
  {
    audioOutput_->suspend();
  }

  close();
}
