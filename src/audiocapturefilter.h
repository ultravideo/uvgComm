#pragma once

#include "filter.h"
#include "audiocapturedevice.h"

#include <QByteArray>


class AudioCaptureFilter : public Filter
{
public:
  AudioCaptureFilter(StatisticsInterface* stats);

  virtual ~AudioCaptureFilter();

  void init();

  void toggleMode();
  void toggleSuspend();
  void deviceChanged(int index);

protected:

  void process();

private:

  void initializeAudio();
  void createAudioInput();

  void readMore();
  void sliderChanged(int value);


  QAudioDeviceInfo deviceInfo_;
  AudioCaptureDevice* device_;
  QAudioFormat format_;
  QAudioInput *audioInput_;
  QIODevice *input_;
  bool pullMode_;
  QByteArray buffer_;
};
