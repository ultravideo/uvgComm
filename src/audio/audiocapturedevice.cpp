#include "audiocapturedevice.h"

#include <QtEndian>
#include <QDebug>

AudioCaptureDevice::AudioCaptureDevice(const QAudioFormat &format, QObject *parent)
  :   QIODevice(parent)
  ,   m_format(format)
  ,   m_maxAmplitude(0)
  ,   m_level(0.0)
{
  switch (m_format.sampleSize()) {
  case 8:
    switch (m_format.sampleType()) {
    case QAudioFormat::UnSignedInt:
      m_maxAmplitude = 255;
      break;
    case QAudioFormat::SignedInt:
      m_maxAmplitude = 127;
      break;
    default:
      break;
    }
    break;
  case 16:
    switch (m_format.sampleType()) {
    case QAudioFormat::UnSignedInt:
      m_maxAmplitude = 65535;
      break;
    case QAudioFormat::SignedInt:
      m_maxAmplitude = 32767;
      break;
    default:
      break;
    }
    break;

  case 32:
    switch (m_format.sampleType()) {
    case QAudioFormat::UnSignedInt:
      m_maxAmplitude = 0xffffffff; // this is why we need 64 bits instead of 32
      break;
    case QAudioFormat::SignedInt:
      m_maxAmplitude = 0x7fffffff;
      break;
    case QAudioFormat::Float:
      m_maxAmplitude = 0x7fffffff; // Kind of
      break;
    default:
      break;
    }
    break;

  default:
    break;
  }
}

AudioCaptureDevice::~AudioCaptureDevice()
{

}

void AudioCaptureDevice::start()
{
  open(QIODevice::WriteOnly);
}

void AudioCaptureDevice::stop()
{
  close();
}

qint64 AudioCaptureDevice::readData(char *data, qint64 maxlen)
{
  Q_UNUSED(data)
  Q_UNUSED(maxlen)

  return 0;
}

qint64 AudioCaptureDevice::writeData(const char *data, qint64 len)
{
  //qDebug() << "Converting capture IOdevice data with size:" << len;
  if (m_maxAmplitude) {
    Q_ASSERT(m_format.sampleSize() % 8 == 0);
    const int channelBytes = m_format.sampleSize() / 8;
    const long long sampleBytes = m_format.channelCount() * channelBytes;
    Q_ASSERT(len % sampleBytes == 0);
    const long long numSamples = len / sampleBytes;

    qint64 maxValue = 0;
    const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);

    for (int i = 0; i < numSamples; ++i) {
      for (int j = 0; j < m_format.channelCount(); ++j) {
        maxValue = qMax(dataToValue(ptr), maxValue);
        ptr += channelBytes;
      }
    }

    maxValue = qMin(maxValue, m_maxAmplitude);
    m_level = qreal(maxValue) / m_maxAmplitude;
  }

  return len;
}

qint64 AudioCaptureDevice::dataToValue(const unsigned char *ptr)
{
  qint64 value = 0;
  if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::UnSignedInt)
  {
    value = *reinterpret_cast<const quint8*>(ptr);
  }
  else if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::SignedInt)
  {
    value = qAbs(*reinterpret_cast<const qint8*>(ptr));
  }
  else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::UnSignedInt)
  {
    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
    {
      value = qFromLittleEndian<quint16>(ptr);
    }
    else
    {
      value = qFromBigEndian<quint16>(ptr);
    }
  }
  else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::SignedInt)
  {
    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
    {
      value = qAbs(qFromLittleEndian<qint16>(ptr));
    }
    else
    {
      value = qAbs(qFromBigEndian<qint16>(ptr));
    }
  }
  else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::UnSignedInt)
  {
    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
    {
      value = qFromLittleEndian<qint32>(ptr);
    }
    else
    {
      value = qFromBigEndian<qint32>(ptr);
    }
  }
  else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::SignedInt)
  {
    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
    {
      value = qAbs(qFromLittleEndian<qint32>(ptr));
    }
    else
    {
      value = qAbs(qFromBigEndian<qint32>(ptr));
    }
  }
  else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::Float)
  {
    // gives the absolute number, expands it and converts the type to int32
    value = static_cast<qint32>(qAbs(*reinterpret_cast<const float*>(ptr) * 0x7fffffff)); // assumes 0-1.0
  }
  return value;
}
