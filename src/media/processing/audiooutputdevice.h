#pragma once

#include <QAudioOutput>
#include <QObject>
#include <QMutex>

#include <stdint.h>
#include <memory>

class Filter;
class StatisticsInterface;
class AECProcessor;
struct Data;

// TODO: There should be an audio buffer with minimum and maximum values so we always have data to send.

class AudioOutputDevice : public QIODevice
{
  Q_OBJECT
public:
  AudioOutputDevice(StatisticsInterface* stats);
  virtual ~AudioOutputDevice();

  void updateSettings();

  void init(QAudioFormat format,
            std::shared_ptr<AECProcessor> AEC);
  void start(); // resume audio output
  void stop(); // suspend audio output

  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;
  qint64 bytesAvailable() const override;

  void addInput()
  {
    ++inputs_;
  }

  void removeInput()
  {
    --inputs_;
  }

  // Receives input from filter graph and tells output that there is input available
  void takeInput(std::unique_ptr<Data> input, uint32_t sessionID);

private:

  void createAudioOutput();

  std::unique_ptr<uchar[]> mixAudio(std::unique_ptr<Data> input, uint32_t sessionID);

  std::unique_ptr<uchar[]> doMixing(uint32_t frameSize);

  void resetBuffers(uint32_t newSize);

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  QMutex mixingMutex_;
  std::map<uint32_t, std::unique_ptr<Data>> mixingBuffer_;

  QMutex sampleMutex_;

  // this will have the next played output audio. The same frame is played
  // if no new frame has been received.
  uint8_t* outputSample_;
  uint32_t sampleSize_;

  uint8_t* partialSample_;
  uint32_t partialSampleSize_;

  unsigned int inputs_;

  std::shared_ptr<AECProcessor> aec_;

  bool mixedSample_;
  unsigned int outputRepeats_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
