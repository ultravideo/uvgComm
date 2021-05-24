#include "audiooutputdevice.h"

#include "filter.h"
#include "statisticsinterface.h"
#include "audioframebuffer.h"

#include "common.h"
#include "global.h"


uint8_t MAX_SAMPLE_REPEATS = 5;

uint8_t MAX_BUFFER_SIZE = AUDIO_FRAMES_PER_SECOND/5;


AudioOutputDevice::AudioOutputDevice():
  QIODevice(),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  audioOutput_(nullptr),
  output_(nullptr),
  format_(),
  buffer_(nullptr),
  latestFrame_(nullptr),
  latestFrameIsSilence_(true),
  outputRepeats_(0)
{}


AudioOutputDevice::~AudioOutputDevice()
{
  audioOutput_->stop();
  destroyLatestFrame();
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

  // it is possible to reduce the buffer size here to reduce latency, but this
  // causes issues with audio reliability with Qt and is not recommended.

  open(QIODevice::ReadOnly);
  // pull mode

  audioOutput_->start(this);

  buffer_ = std::make_unique<AudioFrameBuffer>(audioOutput_->periodSize());

  printDebug(DEBUG_NORMAL, this, "Created audio output",
             {"Notify interval", "Buffer size", "Period Size"},
             {QString::number(audioOutput_->notifyInterval()),
              QString::number(audioOutput_->bufferSize()),
              QString::number(audioOutput_->periodSize())});
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

  if (maxlen < buffer_->getDesiredSize())
  {
    return read;
  }

  // on windows, read as many frames from buffer to output as possible
  // on linux, we only read one sample, since it seems to work better
  uint8_t* frame = buffer_->readFrame();

#ifdef __linux__
  if (frame)
  {
    writeFrame(data, read, frame);
  }

#else
  while (frame)
  {
    writeFrame(data, read, frame);

    if (maxlen - read < buffer_->getDesiredSize())
    {
      break;
    }

    frame = buffer_->readFrame();
  }

#endif

  // make sure buffer doesn't grow too large
  while (MAX_BUFFER_SIZE < buffer_->getBufferSize())
  {
    printWarning(this, "The output device buffer is too large. Dropping audio frames",
                 "Buffer Status",
                 QString::number(buffer_->getBufferSize()) + "/" + QString::number(MAX_BUFFER_SIZE));

    frame = buffer_->readFrame();
    replaceLatestFrame(frame);
  }

  // If we failed to read any frames, we have to put something (previous or
  // an empty frame) into output. Otherwise trouble ensues (Qt stops asking for frames).
  if (read == 0)
  {
    // we have to give output something
    if (latestFrame_ == nullptr)
    {
      printWarning(this, "No output audio frame available in time and no previous frame available. Playing silence");
      // equals to silence
      latestFrame_ = createEmptyFrame(buffer_->getDesiredSize());
      latestFrameIsSilence_ = true;
    }
    else if (outputRepeats_ >= MAX_SAMPLE_REPEATS && !latestFrameIsSilence_)
    {
      printWarning(this, "No output audio frame available in time. Switching to silence");
      destroyLatestFrame();
      latestFrame_ = createEmptyFrame(buffer_->getDesiredSize());
      outputRepeats_ = 0;
      latestFrameIsSilence_ = true;
    }
    else if (!latestFrameIsSilence_)
    {
      printWarning(this, "No output audio frame available in time. Repeating previous audio frame",
      {"Consecutive repeats"}, {QString::number(outputRepeats_ + 1)});
    }

    memcpy(data + read, latestFrame_, buffer_->getDesiredSize());
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


void AudioOutputDevice::replaceLatestFrame(uint8_t* frame)
{
  destroyLatestFrame();
  latestFrame_ = frame;
  outputRepeats_ = 0;
  latestFrameIsSilence_ = false;
}

void AudioOutputDevice::destroyLatestFrame()
{
  if (latestFrame_)
  {
    delete [] latestFrame_;
    latestFrame_ = nullptr;
  }
}

void AudioOutputDevice::writeFrame(char *data, qint64& read, uint8_t* frame)
{
  memcpy(data + read, frame, buffer_->getDesiredSize());
  read += buffer_->getDesiredSize();

  // Delete previous latest frame. This is how the memory is eventually freed
  // for every frame.
  replaceLatestFrame(frame);
}
