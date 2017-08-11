#include "audiooutputdevice.h"

#include <QDebug>
#include <QDateTime>

#include "statisticsinterface.h"


AudioOutputDevice::AudioOutputDevice(StatisticsInterface* stats, uint32_t peer):
  QIODevice(),
  stats_(stats),
  peer_(peer)
{

}

void AudioOutputDevice::init(std::shared_ptr<Filter> source)
{
  Q_ASSERT(source != NULL);

  if(source)
  {
    source->addDataOutCallback(this, &AudioOutputDevice::takeInput);
  }

}

void AudioOutputDevice::start()
{
    open(QIODevice::ReadOnly);
}

void AudioOutputDevice::stop()
{
    close();
}

qint64 AudioOutputDevice::readData(char *data, qint64 len)
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

qint64 AudioOutputDevice::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

qint64 AudioOutputDevice::bytesAvailable() const
{
    return m_buffer.size() + QIODevice::bytesAvailable();
}

void AudioOutputDevice::takeInput(std::unique_ptr<Data> input)
{
  int64_t delay = QDateTime::currentMSecsSinceEpoch() -
      ((uint64_t)input->presentationTime.tv_sec * 1000 + (uint64_t)input->presentationTime.tv_usec/1000);
  stats_->receiveDelay(peer_, "Audio", delay);

  bufferMutex_.lock();
  m_buffer.append((const char*)input->data.get(), input->data_size);
  bufferMutex_.unlock();
  emit inputAvailable();
}
