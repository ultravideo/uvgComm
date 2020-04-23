#pragma once

#include <QAudioOutput>
#include <QObject>
#include <QMutex>

#include <stdint.h>
#include <memory>

class Filter;
class StatisticsInterface;

struct Data;

class AudioOutput : public QObject
{
  Q_OBJECT
public:
  AudioOutput(StatisticsInterface* stats);
  virtual ~AudioOutput();

  void initializeAudio(QAudioFormat format);
  void start(); // resume audio output
  void stop(); // suspend audio output

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

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  QMutex mixingMutex_;
  std::map<uint32_t, std::unique_ptr<Data>> mixingBuffer_;

  unsigned int inputs_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
