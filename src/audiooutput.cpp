#include "audiooutput.h"

#include "audiooutputdevice.h"


#include <QDebug>

const int BufferSize      = 32768;

AudioOutput::AudioOutput(StatisticsInterface *stats, uint32_t peer):
  QObject(),
  stats_(stats),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  source_(0),
  audioOutput_(0),
  output_(0),
  format_(),
  buffer_(BufferSize, 0),
  peer_(peer)
{}

AudioOutput::~AudioOutput()
{}

void AudioOutput::initializeAudio(QAudioFormat format)
{
  QAudioDeviceInfo info(device_);
  if (!info.isFormatSupported(format)) {
    qWarning() << "Default format not supported - trying to use nearest";
    format_ = info.nearestFormat(format);
  }
  else
  {
    format_ = format;
  }

  if(source_)
    delete source_;

  source_ = new AudioOutputDevice(stats_, peer_);

  connect(source_, SIGNAL(inputAvailable()), SLOT(receiveInput()));

  createAudioOutput();
}

void AudioOutput::createAudioOutput()
{
  if(audioOutput_)
  {
    delete audioOutput_;
  }
  audioOutput_ = new QAudioOutput(device_, format_, this);
  source_->start();
  // pull mode
  output_ = audioOutput_->start();
}

void AudioOutput::deviceChanged(int index)
{
  //m_pushTimer->stop();
  source_->stop();
  audioOutput_->stop();
  audioOutput_->disconnect(this);
  //device = m_deviceBox->itemData(index).value<QAudioDeviceInfo>();
  initializeAudio(format_);
}

void AudioOutput::volumeChanged(int value)
{
  if (audioOutput_)
    audioOutput_->setVolume(qreal(value/100.0f));
}

void AudioOutput::receiveInput()
{
  if (audioOutput_ && audioOutput_->state() != QAudio::StoppedState) {
    int chunks = audioOutput_->bytesFree()/audioOutput_->periodSize();
    while (chunks)
    {
      const qint64 len = source_->read(buffer_.data(), audioOutput_->periodSize());
      if (len)
        output_->write(buffer_.data(), len);
      if (len != audioOutput_->periodSize())
        break;
      --chunks;
    }
  }
}

void AudioOutput::start()
{
  if(audioOutput_->state() == QAudio::SuspendedState
     || audioOutput_->state() == QAudio::StoppedState)
  {
    audioOutput_->resume();
  }
}

void AudioOutput::stop()
{
  if(audioOutput_->state() == QAudio::ActiveState)
  {
    audioOutput_->suspend();
  }
}
