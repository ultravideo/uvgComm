#pragma once
#include "filter.h"

#include <QByteArray>
#include <QAudioDeviceInfo>

//TODO: this class would not have to be a filter, just needs to send data to one

class QAudioInput;
class QIODevice;

class AudioCaptureFilter : public Filter
{
  Q_OBJECT
public:
  AudioCaptureFilter(QString id, QAudioFormat format, StatisticsInterface* stats);
  virtual ~AudioCaptureFilter();

  virtual bool init(); // setups audio device and parameters.
  virtual void updateSettings(); // changes the selected audio device
  virtual void start(); // resumes audio input
  virtual void stop(); // suspends audio input

protected:

  // this does nothing. ReadMore does the sending of
  void process();

private slots:

  // reads audio sample data
  void readMore();

  void volumeChanged(int value);

private:

  void createAudioInput();

  QAudioDeviceInfo deviceInfo_;
  QAudioFormat format_;
  QAudioInput *audioInput_;
  QIODevice *input_;
  bool pullMode_;
  QByteArray buffer_;
};
