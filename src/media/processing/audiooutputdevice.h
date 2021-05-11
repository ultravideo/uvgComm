#pragma once

#include <QAudioOutput>
#include <QObject>
#include <QMutex>

#include <stdint.h>
#include <memory>
#include <deque>

class StatisticsInterface;
struct Data;

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

  void resetBuffers(uint32_t newSize);
  void deleteBuffers();

  void addSampleToBuffer(uint8_t* sample, int sampleSize);

  uint8_t* createEmptyFrame(uint32_t size);

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  QMutex bufferMutex_;

  // this will have the next played output audio. The same frame is played
  // if no new frame has been received.
  std::deque<uint8_t*> outputBuffer_;
  uint32_t sampleSize_;

  QMutex partialMutex_;
  uint8_t* partialSample_;
  int partialSampleSize_;

  unsigned int outputRepeats_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
