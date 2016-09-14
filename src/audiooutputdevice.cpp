#include "audiooutputdevice.h"

#include <QDebug>

AudioOutputDevice::AudioOutputDevice(StatisticsInterface* stats):
  QIODevice(),
  pos_(0)
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
    pos_ = 0;
    close();
}

qint64 AudioOutputDevice::readData(char *data, qint64 len)
{
    qint64 total = 0;
    if (!m_buffer.isEmpty()) {
        while (len - total > 0) {
            const qint64 chunk = qMin((m_buffer.size() - pos_), len - total);
            memcpy(data + total, m_buffer.constData() + pos_, chunk);
            pos_ = (pos_ + chunk) % m_buffer.size();
            total += chunk;
        }
    }
    return total;
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
  m_buffer.append((const char*)input->data.get(), input->data_size);
  emit inputAvailable();
}
