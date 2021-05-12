#pragma once

#include <QAudioOutput>
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

  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;
  qint64 bytesAvailable() const override;

  // input one frame of audio
  void input(std::unique_ptr<Data> input);

private:

  void createAudioOutput();

  uint8_t* createEmptyFrame(uint32_t size);

  void destroyLatestSample();

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  std::unique_ptr<AudioFrameBuffer> buffer_;

  uint8_t* latestSample_;

  unsigned int outputRepeats_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
