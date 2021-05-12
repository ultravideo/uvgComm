#include "audiooutputdevice.h"

#include "filter.h"
#include "statisticsinterface.h"
#include "audioframebuffer.h"

#include "common.h"
#include "global.h"


uint8_t MAX_SAMPLE_REPEATS = 5;

#ifdef __linux__
uint8_t MAX_BUFFER_SAMPLES = AUDIO_FRAMES_PER_SECOND/2;
#else
uint8_t MAX_BUFFER_SAMPLES = AUDIO_FRAMES_PER_SECOND/5;
#endif

AudioOutputDevice::AudioOutputDevice():
  QIODevice(),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  audioOutput_(nullptr),
  output_(nullptr),
  format_(),
  buffer_(nullptr),
  latestSample_(nullptr),
  outputRepeats_(0)
{}


AudioOutputDevice::~AudioOutputDevice()
{
  audioOutput_->stop();
  destroyLatestSample();
}


void AudioOutputDevice::init(QAudioFormat format)
{
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
  if(audioOutput_)
  {
    delete audioOutput_;
  }
  audioOutput_ = new QAudioOutput(device_, format_, this);

  //sampleSize_ = format_.sampleRate()*format_.bytesPerFrame()/AUDIO_FRAMES_PER_SECOND;

  open(QIODevice::ReadOnly);
  // pull mode

  audioOutput_->start(this);

  buffer_ = std::make_unique<AudioFrameBuffer>(audioOutput_->periodSize());
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
  qint64 read = 0;

  // make sure we are giving the correct size audio frames
  if (audioOutput_->periodSize() != buffer_->getDesiredSize())
  {
    buffer_->changeDesiredFrameSize(audioOutput_->periodSize());
  }

  // read as many frames from buffer to output as possible
  uint8_t* frame = buffer_->readFrame();
  while (maxlen - read >= buffer_->getDesiredSize() && frame)
  {
    memcpy(data + read, frame, buffer_->getDesiredSize());
    read += buffer_->getDesiredSize();

    destroyLatestSample();
    latestSample_ = frame;
    outputRepeats_ = 0;

    frame  = buffer_->readFrame();
  }

  // if we failed to read any frames, put something (like previous sample or empty sample)
  // into output. Otherwise trouble ensues (it stops asking for frames)
  if (read == 0)
  {
    printWarning(this, "No output audio frame available in time");

    // we have to give output something
    if (latestSample_ == nullptr)
    {
      // equals to silence
      latestSample_ = createEmptyFrame(buffer_->getDesiredSize());
    }
    else if (outputRepeats_ >= MAX_SAMPLE_REPEATS)
    {
      destroyLatestSample();
      latestSample_ = createEmptyFrame(buffer_->getDesiredSize());
      outputRepeats_ = 0;
    }

    memcpy(data + read, latestSample_, buffer_->getDesiredSize());
    read += buffer_->getDesiredSize();

    // keep track of repeats so we can switch to silence at some point.
    ++outputRepeats_;
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
  return buffer_->getDesiredSize() + QIODevice::bytesAvailable();
}


void AudioOutputDevice::input(std::unique_ptr<Data> input)
{
  if (input->type != RAWAUDIO)
  {
    printProgramError(this, "Audio output has received something other than raw audio!");
    return;
  }

  if (audioOutput_ && audioOutput_->state() != QAudio::StoppedState)
  {
    // buffer handles the correct size of the audio frame for output
    buffer_->inputData(input->data.get(), input->data_size);
  }
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


uint8_t* AudioOutputDevice::createEmptyFrame(uint32_t size)
{
  uint8_t* emptyFrame  = new uint8_t[size];
  memset(emptyFrame, 0, size);
  return emptyFrame;
}


void AudioOutputDevice::destroyLatestSample()
{
  if (latestSample_)
  {
    delete [] latestSample_;
    latestSample_ = nullptr;
  }
}
