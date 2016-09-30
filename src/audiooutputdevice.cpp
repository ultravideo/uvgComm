#include "audiooutputdevice.h"

#include <QDebug>

AudioOutputDevice::AudioOutputDevice(StatisticsInterface* stats):
  QIODevice()
{

}

void AudioOutputDevice::init(Filter* source)
{
  Q_ASSERT(source);

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
  bufferMutex_.lock();
  m_buffer.append((const char*)input->data.get(), input->data_size);
  bufferMutex_.unlock();
  emit inputAvailable();
}
