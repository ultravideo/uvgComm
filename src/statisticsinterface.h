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
  virtual void delayTime(QString type, uint32_t delay) = 0;
  virtual void addEncodedVideo(uint16_t size) = 0;
  virtual void addSendPacket(uint16_t size) = 0;
  virtual void addReceivePacket(uint16_t size) = 0;
  virtual void updateBufferStatus(QString filter, uint16_t buffersize) = 0;
  virtual void packetDropped() = 0;

protected:

  // used for multiple statistics interfaces
  StatisticsInterface* next_;
};
