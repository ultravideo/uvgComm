#pragma once

#include "filter.h"
#include "audiocapturedevice.h"

#include <QByteArray>

//TODO: this class would not have to be a filter, just needs to send data to one

class AudioCaptureFilter : public Filter
{
  Q_OBJECT
public:
  AudioCaptureFilter(StatisticsInterface* stats);

  virtual ~AudioCaptureFilter();

  void initializeAudio(QAudioFormat format);

  virtual void start();
  virtual void stop();

protected:

  void process();

private slots:

  void readMore();
  void volumeChanged(int value);

private:

  void deviceChanged(int index);
  void createAudioInput();

  QAudioDeviceInfo deviceInfo_;
  AudioCaptureDevice* device_;
  QAudioFormat format_;
  QAudioInput *audioInput_;
  QIODevice *input_;
  bool pullMode_;
  QByteArray buffer_;
};
