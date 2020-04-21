#include "audiooutput.h"

#include "filter.h"
#include "statisticsinterface.h"

#include "common.h"

#include <QDateTime>

const int BUFFER_SIZE = 32768;

AudioOutput::AudioOutput(StatisticsInterface *stats, uint32_t peer):
  QObject(),
  stats_(stats),
  device_(QAudioDeviceInfo::defaultOutputDevice()),
  audioOutput_(nullptr),
  output_(nullptr),
  format_(),
  buffer_(BUFFER_SIZE, 0),
  peer_(peer)
{}


AudioOutput::~AudioOutput()
{}


void AudioOutput::initializeAudio(QAudioFormat format, std::shared_ptr<Filter> source)
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

  connect(this, SIGNAL(inputAvailable()), SLOT(receiveInput()));

  createAudioOutput();

  Q_ASSERT(source != nullptr);

  if(source)
  {
    source->addDataOutCallback(this, &AudioOutput::takeInput);
  }
}


void AudioOutput::createAudioOutput()
{
  if(audioOutput_)
  {
    delete audioOutput_;
  }
  audioOutput_ = new QAudioOutput(device_, format_, this);

  // pull mode
  output_ = audioOutput_->start();
}


void AudioOutput::deviceChanged(int index)
{
  Q_UNUSED(index);
  printUnimplemented(this, "Audio output device change not implemented fully.");

  audioOutput_->stop();
  audioOutput_->disconnect(this);
  //initializeAudio(format_);
}


void AudioOutput::volumeChanged(int value)
{
  if (audioOutput_)
    audioOutput_->setVolume(qreal(value/100.0f));
}


void AudioOutput::takeInput(std::unique_ptr<Data> input)
{
  int64_t delay = QDateTime::currentMSecsSinceEpoch() -
      ((uint64_t)input->presentationTime.tv_sec * 1000 +
       (uint64_t)input->presentationTime.tv_usec/1000);
  stats_->receiveDelay(peer_, "Audio", delay);

  bufferMutex_.lock();
  m_buffer.append((const char*)input->data.get(), input->data_size);
  bufferMutex_.unlock();
  emit inputAvailable();
}


void AudioOutput::receiveInput()
{
  if (audioOutput_ && audioOutput_->state() != QAudio::StoppedState)
  {
    int chunks = audioOutput_->bytesFree()/audioOutput_->periodSize();

    while (chunks)
    {
      const qint64 len = readData(buffer_.data(), audioOutput_->periodSize());
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


qint64 AudioOutput::readData(char *data, qint64 len)
{
  bufferMutex_.lock();
  const qint64 chunk = qMin((qint64)m_buffer.size(), len);

  if (!m_buffer.isEmpty())
  {
    memcpy(data, m_buffer.constData(), chunk);
    m_buffer.remove(0,chunk);
  }
  bufferMutex_.unlock();
  return chunk;
}
