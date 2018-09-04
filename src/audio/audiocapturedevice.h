#pragma once

// This class has been modified from QT audio input example
#include <QAudioFormat>
#include <QIODevice>

// Honestly this whole class seems useless. It only calculates the audiolevel which is not used anywhere
// but since its not harming anyone, I'll leave it here for now.
// this class was originally copied from an example.

class AudioCaptureDevice :  public QIODevice
{
    Q_OBJECT
public:
    AudioCaptureDevice(const QAudioFormat &format, QObject *parent);
    ~AudioCaptureDevice();

    // opens the audio device
    void start();

    // closes the audio device
    void stop();

    // the audio level, maybe show it in settings?
    qreal level() const { return m_level; }

    // does nothing
    qint64 readData(char *data, qint64 maxlen);

    // calculate the current level.
    qint64 writeData(const char *data, qint64 len);

private:

    // a helper function to select correct interpretation of the data
    qint64 dataToValue(const unsigned char *ptr);

    const QAudioFormat m_format;
    qint64 m_maxAmplitude;
    qreal m_level; // 0.0 <= m_level <= 1.0

};
