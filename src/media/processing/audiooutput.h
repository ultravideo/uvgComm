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
  AudioOutput(StatisticsInterface* stats, uint32_t peer);
  virtual ~AudioOutput();

  void initializeAudio(QAudioFormat format, std::shared_ptr<Filter> source);
  void start(); // resume audio output
  void stop(); // suspend audio output

  // Receives input from filter graph and tells output that there is input available
  void takeInput(std::unique_ptr<Data> input);

signals:
  void inputAvailable();

private:

  // read data from buffer
  qint64 readData(char *data, qint64 maxlen);

  void createAudioOutput();

  StatisticsInterface* stats_;

  QByteArray m_buffer;
  QMutex bufferMutex_;

  QAudioDeviceInfo device_;
  QAudioOutput *audioOutput_;
  QIODevice *output_; // not owned
  QAudioFormat format_;

  QByteArray buffer_;
  uint32_t peer_;

private slots:
  void receiveInput();
  void deviceChanged(int index);
  void volumeChanged(int);
};
