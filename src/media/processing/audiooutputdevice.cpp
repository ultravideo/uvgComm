#include "audiooutputdevice.h"

#include "filter.h"
#include "statisticsinterface.h"
#include "aecprocessor.h"

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
  inputs_(0),
  mixedSample_(false),
  outputRepeats_(0)
{}


AudioOutputDevice::~AudioOutputDevice()
{}


void AudioOutputDevice::init(QAudioFormat format,
                             std::shared_ptr<AECProcessor> AEC)
{
  aec_ = AEC;

  QAudioDeviceInfo info(device_);
  if (!info.isFormatSupported(format)) {
    printDebug(DEBUG_WARNING, this,
               "Default format not supported - trying to use nearest.");
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
  if (!aec_)
  {
    printProgramError(this, "AEC not set");
  }

  if(audioOutput_)
  {
    delete audioOutput_;
  }
  audioOutput_ = new QAudioOutput(device_, format_, this);

  sampleSize_ = format_.sampleRate()*format_.bytesPerFrame()/AUDIO_FRAMES_PER_SECOND;
  outputSample_ = aec_->createEmptyFrame(sampleSize_);

  open(QIODevice::ReadOnly);
  // pull mode
  audioOutput_->start(this);
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
    if (audioOutput_->periodSize() > sampleSize_)
    {
      printWarning(this, "The audio Frame size is too small. "
                         "Buffer output underflow.", {"PeriodSize vs frame size"}, {
                     QString::number(audioOutput_->periodSize()) + " vs " +
                     QString::number(sampleSize_)});
    }

    if (!aec_)
    {
      printProgramError(this, "AEC not set");
    }

    // if no new input has arrived, we play the last sample
    sampleMutex_.lock();

    // we keep track if this is a repeat so we can change it to silence.
    if (!mixedSample_)
    {
      ++outputRepeats_;
    }
    else
    {
      mixedSample_ = false;
    }

    // start playing silence if we played last frame three times.
    if (outputRepeats_ == 3)
    {
      outputSample_ = aec_->createEmptyFrame(sampleSize_);
    }

    // send sample to AEC
    std::shared_ptr<uchar[]> echoSample = outputSample_;
    aec_->processEchoFrame(echoSample, sampleSize_);

    // send sample to speakers
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
    mixedSample_ = true;
    outputRepeats_ = 0;
    sampleSize_ = dataLeft;
    outputSample_ = std::move(outputFrame);
    sampleMutex_.unlock();
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
