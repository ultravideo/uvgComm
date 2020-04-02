#pragma once
#include <QString>
#include <QSize>

#include <map>
#include <stdint.h>

// An interface where the program tells various statistics of its operations.
// Can be used to show the statistics in window or to record the statistics to a file.

// TODO: improve the interface to add the participant details in pieces.

class StatisticsInterface
{
public:
  virtual ~StatisticsInterface(){}

  // MEDIA SESSION
  // add/remove session information or log the start/end of session.
  virtual void addSession(uint32_t sessionID) = 0;
  virtual void removeSession(uint32_t sessionID) = 0;

  // MEDIA
  // basic information on audio/video. Can be called in case information changes.
  virtual void videoInfo(double framerate, QSize resolution) = 0;
  virtual void audioInfo(uint32_t sampleRate, uint16_t channelCount) = 0;

  // basic call info for incoming and outgoing medias
  virtual void incomingMedia(uint32_t sessionID, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts) = 0;
  virtual void outgoingMedia(uint32_t sessionID, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts) = 0;

  // the delay it takes from input to the point when input is encoded
  virtual void sendDelay(QString type, uint32_t delay) = 0;

  // the delay until the presentation of the packet
  virtual void receiveDelay(uint32_t sessionID, QString type, int32_t delay) = 0;

  // one packet has been presented to user
  virtual void presentPackage(uint32_t sessionID, QString type) = 0;

  // For tracking of encoding bitrate and possibly other information.
  virtual void addEncodedPacket(QString type, uint32_t size) = 0;



  // DELIVERY
  // Tracking of sent packets
  virtual void addSendPacket(uint16_t size) = 0;

  // tracking of received packets.
  virtual void addReceivePacket(uint16_t size) = 0;


  // FILTER
  // tell the that we want to track this filter or stop tracking
  virtual void addFilter(QString filter, uint64_t TID) = 0;
  virtual void removeFilter(QString filter) = 0;

  // Tracking of buffer information.
  virtual void updateBufferStatus(QString filter, uint16_t buffersize, uint16_t maxBufferSize) = 0;

  // Tracking of packets dropped due to buffer overflow
  virtual void packetDropped(QString filter) = 0;


  // SIP
  // Tracking of sent and received SIP Messages
  virtual void addSentSIPMessage(QString type, QString message, QString address) = 0;
  virtual void addReceivedSIPMessage(QString type, QString message, QString address) = 0;
};
