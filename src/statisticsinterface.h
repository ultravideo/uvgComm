#pragma once

#include <QString>
#include <QSize>

#include <map>

#include <sys/time.h>

class StatisticsInterface
{
public:
  StatisticsInterface(){}

  virtual void addNextInterface(StatisticsInterface* next) = 0;

  virtual void videoInfo(double framerate, QSize resolution) = 0;
  virtual void audioInfo(uint32_t sampleRate) = 0;

  virtual void addParticipant(QString ip, QString port) = 0;

  virtual void delayTime(QString type, struct timeval) = 0;

  virtual void addSendPacket(uint16_t size) = 0;

  virtual void updateBufferStatus(QString filter, uint16_t buffersize) = 0;

protected:

  // used for multiple statistics interfaces
  StatisticsInterface* next_;

  std::map<QString, uint16_t> buffers_;
};
