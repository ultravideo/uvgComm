#include "audiooutputdevice.h"

#include "filter.h"
#include "statisticsinterface.h"
#include "speexdsp.h"

#include "common.h"
#include "global.h"

#include <QDateTime>

#include <QDebug>

uint8_t MAX_SAMPLE_REPEATS = 5;
uint8_t MAX_BUFFER_SAMPLES = 4;

AudioOutputDevice::AudioOutputDevice(StatisticsInterface *stats):
  QIODevice(),
  stats_(stats),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  audioOutput_(nullptr),
  output_(nullptr),
  format_(),
  sampleMutex_(),
  outputBuffer_(),
  sampleSize_(0),
  partialSample_(nullptr),
  partialSampleSize_(0),
  inputs_(0),
  mixedSample_(false),
  outputRepeats_(0)
{}


AudioOutputDevice::~AudioOutputDevice()
{
  deleteBuffers();
  audioOutput_->stop();
}


void AudioOutputDevice::init(QAudioFormat format,
                             std::shared_ptr<SpeexDSP> AEC)
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

  //sampleSize_ = format_.sampleRate()*format_.bytesPerFrame()/AUDIO_FRAMES_PER_SECOND;

  open(QIODevice::ReadOnly);
  // pull mode

  sampleMutex_.lock();
  audioOutput_->start(this);

  resetBuffers(audioOutput_->periodSize());
  sampleMutex_.unlock();
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
    printWarning(this, "Output wants smaller sample than what we use. "
                        "Part of the sample may be left unplayed",
                 {"Read vs available"},
                 {QString::number(maxlen) + " vs " + QString::number(sampleSize_)});
  }
  else
  {
    if (audioOutput_->periodSize() > sampleSize_)
    {
      printWarning(this, "We use smaller sample than what audio output wants. "
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
    if (outputRepeats_ >= MAX_SAMPLE_REPEATS || outputBuffer_.empty())
    {
      printWarning(this, "Resetting audio buffers since there is not enough samples");
      resetBuffers(sampleSize_);
      outputRepeats_ = 0;
    }

    // take oldest sample in buffer
    uint8_t* sample = outputBuffer_.front();

    // send sample to AEC to use as a reference of echo
    aec_->processEchoFrame(sample, sampleSize_);

    // send sample to speakers
    memcpy(data, sample, sampleSize_);
    read = sampleSize_;

    // delete sample from our buffer if it was not the only sample we have
    if (outputBuffer_.size() > 1)
    {
      delete outputBuffer_.front();
      outputBuffer_.pop_front();
    }

    // if we have too may audio samples stored
    while (outputBuffer_.size() > MAX_BUFFER_SAMPLES)
    {
      printWarning(this, "Audio buffer has grown too large. "
                         "Deleting oldest samples to avoid latency");
      delete outputBuffer_.front();
      outputBuffer_.pop_front();
    }

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
    int64_t delay = QDateTime::currentMSecsSinceEpoch() - input->presentationTime;

    stats_->receiveDelay(sessionID, "Audio", delay);

    int inputSize = input->data_size;

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

    if (sampleSize_ == inputSize) // input is exactly right
    {
      addSampleToBuffer(outputFrame.get(), sampleSize_);
    }
    else if (sampleSize_ > inputSize) // the input sample is smaller than what output wants
    {
      if (partialSampleSize_ + inputSize == sampleSize_)
      {
        memcpy(partialSample_ + partialSampleSize_, outputFrame.get(), inputSize);
        addSampleToBuffer(partialSample_, sampleSize_);
      }
      else if (partialSampleSize_ + inputSize < sampleSize_)
      {
        // we did not update the sample so no reset for output repeats
        // add this partial sample to temporary storage
        memcpy(partialSample_ + partialSampleSize_, outputFrame.get(), inputSize);
      }
      else // if we somehow have more audio data than one sample needs
      {
        uint32_t missingSize = sampleSize_ - partialSampleSize_;

        // add missing piece from new sample
        memcpy(partialSample_ + partialSampleSize_, outputFrame.get(), missingSize);

        // copy finished sample to output
        addSampleToBuffer(partialSample_, sampleSize_);

        // store remaining piece from new sample
        memcpy(partialSample_, outputFrame.get() + missingSize, inputSize - missingSize);
        partialSampleSize_ = inputSize - missingSize;
      }
    }
    // If the input frames are larger than output sample,
    // we will have to wait until output wants this much data.
    else if (sampleSize_ < inputSize)
    {
      printDebug(DEBUG_NORMAL, this, "Incoming audio sample is larger than what output wants",
                  {"Incoming sample", "What output wants"}, {
                   QString::number(inputSize), QString::number(sampleSize_)});

      // increase size of buffers
      resetBuffers(inputSize);

      // add new sample to bigger buffers
      addSampleToBuffer(outputFrame.get(), sampleSize_);
    }

    sampleMutex_.unlock();
  }
}


std::unique_ptr<uchar[]> AudioOutputDevice::mixAudio(std::unique_ptr<Data> input,
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
#ifdef __linux__
    audioOutput_->start();
#else
    audioOutput_->resume();
#endif
  }
}


void AudioOutputDevice::stop()
{
  if(audioOutput_->state() == QAudio::ActiveState)
  {
#ifdef __linux__
    audioOutput_->stop();
#else
    audioOutput_->suspend();
#endif
  }

  close();
}


void AudioOutputDevice::deleteBuffers()
{
  for (auto& sample : outputBuffer_)
  {
    if (sample != nullptr)
    {
      delete[] sample;
      sample = nullptr;
    }
  }

  outputBuffer_.clear();
  sampleSize_ = 0;

  if (partialSample_ != nullptr)
  {
    delete[] partialSample_;
    partialSample_ = nullptr;
  }

  partialSampleSize_ = 0;
}


void AudioOutputDevice::resetBuffers(uint32_t newSize)
{
  deleteBuffers();

  sampleSize_ = newSize;
  partialSampleSize_ = 0;
  outputBuffer_.push_back(aec_->createEmptyFrame(sampleSize_));
  partialSample_ = aec_->createEmptyFrame(sampleSize_);
}


void AudioOutputDevice::addSampleToBuffer(uint8_t *sample, int sampleSize)
{
  uint8_t* new_sample = new uint8_t[sampleSize];
  memcpy(new_sample, sample, sampleSize);
  outputBuffer_.push_back(new_sample);

  outputRepeats_ = 0;
}
