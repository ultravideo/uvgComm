#pragma once

#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QAudioFormat>
#include <QIODevice>
#include <QObject>
#include <QMutex>

#include <stdint.h>
#include <memory>
#include <deque>

class StatisticsInterface;
struct Data;
class AudioFrameBuffer;

class AudioOutputDevice : public QIODevice
{
  Q_OBJECT
public:
  AudioOutputDevice();
  virtual ~AudioOutputDevice();

  void init(QAudioFormat format);
  void start(); // resume audio output
  void stop(); // suspend audio output

  void setMutingState(bool state)
  {
    muting_ = state;
  }

  void setMutingThreshold(float threshold)
  {
    mutingThreshold_ = threshold;
  }

  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;
  qint64 bytesAvailable() const override;

  // input one frame of audio
  void input(std::unique_ptr<Data> input);

signals:

  void outputtingSound();

private:

  void createAudioOutput();

  uint8_t* createEmptyFrame(uint32_t size);

  void replaceLatestFrame(uint8_t* frame);

  void destroyLatestFrame();

  void writeFrame(char *data, qint64& read, uint8_t* frame);

  bool isLoud(int16_t* data, uint32_t size);

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  std::unique_ptr<AudioFrameBuffer> buffer_;

  uint8_t* latestFrame_;
  bool latestFrameIsSilence_;

  unsigned int outputRepeats_;

  bool muting_;
  float mutingThreshold_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
