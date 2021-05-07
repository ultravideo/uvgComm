#pragma once

#include <QAudioOutput>
#include <QObject>
#include <QMutex>

#include <stdint.h>
#include <memory>
#include <deque>

#include "audiomixer.h"

class Filter;
class StatisticsInterface;
class SpeexDSP;
struct Data;

// TODO: There should be an audio buffer with minimum and maximum values so we always have data to send.

class AudioOutputDevice : public QIODevice
{
  Q_OBJECT
public:
  AudioOutputDevice(StatisticsInterface* stats);
  virtual ~AudioOutputDevice();

  void init(QAudioFormat format,
            std::shared_ptr<SpeexDSP> AEC);
  void start(); // resume audio output
  void stop(); // suspend audio output

  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;
  qint64 bytesAvailable() const override;

  void addInput()
  {
    ++inputs_;
    mixer_.setNumberOfInputs(inputs_);
  }

  void removeInput()
  {
    --inputs_;
    mixer_.setNumberOfInputs(inputs_);
  }

  // Receives input from filter graph and tells output that there is input available
  void takeInput(std::unique_ptr<Data> input, uint32_t sessionID);

private:

  void createAudioOutput();

  void resetBuffers(uint32_t newSize);
  void deleteBuffers();

  void addSampleToBuffer(uint8_t* sample, int sampleSize);

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  QMutex sampleMutex_;

  AudioMixer mixer_;

  // this will have the next played output audio. The same frame is played
  // if no new frame has been received.
  std::deque<uint8_t*> outputBuffer_;
  int sampleSize_;

  uint8_t* partialSample_;
  int partialSampleSize_;

  unsigned int inputs_;

  std::shared_ptr<SpeexDSP> aec_;

  unsigned int outputRepeats_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
