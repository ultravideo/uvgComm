#pragma once

#include "filter.h"
#include "audiocapturedevice.h"

#include <QByteArray>


class AudioCaptureFilter : public Filter
{
public:
  AudioCaptureFilter();

  virtual ~AudioCaptureFilter();


  void init();

  virtual bool isInputFilter() const
  {
    return true;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

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
