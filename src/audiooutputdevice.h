#pragma once


#include <QIODevice>
#include "filter.h"

class AudioOutputDevice : public QIODevice
{
 Q_OBJECT
public:
  AudioOutputDevice(StatisticsInterface* stats);

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
  qint64 pos_;
  QByteArray m_buffer;
};
