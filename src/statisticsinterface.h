#pragma once

#include <QString>
#include <QSize>

#include <map>

#include <sys/time.h>

class StatisticsInterface
{
public:
  StatisticsInterface(){}

  // Next interface if there are more than one way of processing statistics
  virtual void addNextInterface(StatisticsInterface* next) = 0;

  // basic information on video. Can be called in case information changes.
  virtual void videoInfo(double framerate, QSize resolution) = 0;

  // basic information on audio. Can be called in case information changes.
  virtual void audioInfo(uint32_t sampleRate, uint16_t channelCount) = 0;

  // adds participant information. May in future be extended to include
  // RTP tracking fo each participant individually with ID
  virtual void addParticipant(QString ip, QString port) = 0;

  // can be used to record delays at different points within program
  virtual void delayTime(QString type, uint32_t delay) = 0;

  // For tracking of encoding bitrate and possibly other information.
  virtual void addEncodedPacket(QString type, uint16_t size) = 0;

  // Tracking of sent packets
  virtual void addSendPacket(uint16_t size) = 0;

  // tracking of received packets.
  virtual void addReceivePacket(uint16_t size) = 0;

  // Tracking of buffer information.
  virtual void updateBufferStatus(QString filter, uint16_t buffersize) = 0;

  // Tracking of packets dropped due to buffer overflow
  virtual void packetDropped() = 0;
};
