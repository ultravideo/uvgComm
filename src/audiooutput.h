#pragma once

#include <QAudioOutput>
#include <QObject>

class AudioOutputDevice;
class StatisticsInterface;

class AudioOutput : public QObject
{
  Q_OBJECT
public:
  AudioOutput(StatisticsInterface* stats);
  virtual ~AudioOutput();

  void initializeAudio(QAudioFormat format);

  AudioOutputDevice* getOutputModule()
  {
    return source_;
  }

private:

  void createAudioOutput();

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  AudioOutputDevice *source_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  QByteArray buffer_;

private slots:
  void receiveInput();
  void toggleSuspendResume();
  void deviceChanged(int index);
  void volumeChanged(int);
};
