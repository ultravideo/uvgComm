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

  // Receives input from filter graph and tells output that there is input available
  void takeInput(std::unique_ptr<Data> input, uint32_t sessionID);

private:

  void createAudioOutput();

  StatisticsInterface* stats_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

private slots:
  void deviceChanged(int index);
  void volumeChanged(int);
};
