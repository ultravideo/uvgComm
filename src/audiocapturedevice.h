#pragma once

// This class has been modified from QT audio input example

#include <QAudioInput>

class AudioCaptureDevice :  public QIODevice
{
    Q_OBJECT
public:
    AudioCaptureDevice(const QAudioFormat &format, QObject *parent);
    ~AudioCaptureDevice();
    void start();
    void stop();

    qreal level() const { return m_level; }

    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

private:
    const QAudioFormat m_format;
    quint32 m_maxAmplitude;
    qreal m_level; // 0.0 <= m_level <= 1.0

};
