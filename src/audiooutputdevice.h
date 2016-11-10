#pragma once

#include "filter.h"

#include <QIODevice>
#include <QMutex>

class AudioOutputDevice : public QIODevice
{
 Q_OBJECT
public:
  AudioOutputDevice(StatisticsInterface* stats, uint32_t peer);

  void init(Filter* source);

  void start();
  void stop();

  // read data from buffer
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len); // unused
  qint64 bytesAvailable() const;

  // Receives input from filter graph and tells output that there is input available
  void takeInput(std::unique_ptr<Data> input);

signals:
  void inputAvailable();

private:
  QByteArray m_buffer;

  QMutex bufferMutex_;

  StatisticsInterface* stats_;

  uint32_t peer_;
};
