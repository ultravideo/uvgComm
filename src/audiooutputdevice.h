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

  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
  qint64 bytesAvailable() const;

  void takeInput(std::unique_ptr<Data> input);

signals:
  void inputAvailable();

private:
  QByteArray m_buffer;

  QMutex bufferMutex_;

  StatisticsInterface* stats_;

  uint32_t peer_;
};
