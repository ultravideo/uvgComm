#pragma once

#include <QString>
#include <QSize>

#include <map>
#include <stdint.h>
#include <vector>

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
  virtual void incomingMedia(uint32_t sessionID, QString name, QStringList& ipList,
                             QStringList& audioPorts, QStringList& videoPorts) = 0;
  virtual void outgoingMedia(uint32_t sessionID, QString name, QStringList& ipList,
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
  virtual void addSendPacket(uint32_t size) = 0;

  // tracking of received packets.
  virtual void addReceivePacket(uint32_t sessionID, QString type, uint32_t size) = 0;

  // Details of an individual packet that shows how well our data is getting delivered
  virtual void addRTCPPacket(uint32_t sessionID, QString type,
                             uint8_t  fraction,
                             int32_t  lost,
                             uint32_t last_seq,
                             uint32_t jitter) = 0;


  // FILTER
  // tell the that we want to track this filter or stop tracking
  virtual uint32_t addFilter(QString type, QString identifier, uint64_t TID) = 0;
  virtual void removeFilter(uint32_t id) = 0;

  // Tracking of buffer information.
  virtual void updateBufferStatus(uint32_t id, uint16_t buffersize,
                                  uint16_t maxBufferSize) = 0;

  // Tracking of packets dropped due to buffer overflow
  virtual void packetDropped(uint32_t id) = 0;


  // SIP
  // Tracking of sent and received SIP Messages
  virtual void addSentSIPMessage(const QString& headerType, const QString& header,
                                 const QString& bodyType,   const QString& body) = 0;
  virtual void addReceivedSIPMessage(const QString& headerType, const QString& header,
                                     const QString& bodyType,   const QString& body) = 0;
};
