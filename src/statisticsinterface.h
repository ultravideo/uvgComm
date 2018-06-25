#pragma once
#include <QString>
#include <QSize>

#include <map>
#include <stdint.h>

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
  virtual void addParticipant(QString ip, QString audioPort, QString videoPort) = 0;

  // adds participant information. May in future be extended to include
  // RTP tracking fo each participant individually with ID
  virtual void removeParticipant(QString ip) = 0;

  // the delay it takes from input to the point when input is encoded
  virtual void sendDelay(QString type, uint32_t delay) = 0;

  // the delay until the presentation of the packet
  virtual void receiveDelay(uint32_t peer, QString type, int32_t delay) = 0;

  // one packet has been presented to user
  virtual void presentPackage(uint32_t peer, QString type) = 0;

  // For tracking of encoding bitrate and possibly other information.
  virtual void addEncodedPacket(QString type, uint16_t size) = 0;

  // Tracking of sent packets
  virtual void addSendPacket(uint16_t size) = 0;

  // tracking of received packets.
  virtual void addReceivePacket(uint16_t size) = 0;

  // tell the that we want to track this filter.
  virtual void addFilter(QString filter, uint64_t TID) = 0;

  // removes tracking for this filter.
  virtual void removeFilter(QString filter) = 0;

  // Tracking of buffer information.
  virtual void updateBufferStatus(QString filter, uint16_t buffersize, uint16_t maxBufferSize) = 0;

  // Tracking of packets dropped due to buffer overflow
  virtual void packetDropped(QString filter) = 0;
};
