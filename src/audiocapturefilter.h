#pragma once

#include "filter.h"
#include "audiocapturedevice.h"

#include <QByteArray>


class AudioCaptureFilter : public Filter
{
public:
  AudioCaptureFilter();

  virtual bool isInputFilter() const
  {
    return true;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

  void initializeAudio();
  void createAudioInput();

protected:
  void process();


private:
  void readMore();
  void toggleMode();
  void toggleSuspend();
  void deviceChanged(int index);
  void sliderChanged(int value);


  QAudioDeviceInfo m_device;
  AudioCaptureDevice device_;
  QAudioFormat m_format;
  QAudioInput *m_audioInput;
  QIODevice *m_input;
  bool m_pullMode;
  QByteArray m_buffer;
};
