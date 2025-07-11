#pragma once

#include <QString>
#include <QSize>

#include <stdint.h>
#include <memory>

// An interface where the program tells various statistics of its operations.
// Can be used to show the statistics in window or to record the statistics to a file.

// TODO: improve the interface to add the participant details in pieces.

struct ICEPair;

class StatisticsInterface
{
public:
  virtual ~StatisticsInterface(){}

  // MEDIA SESSION
  // add/remove session information or log the start/end of session.
  virtual void addSession(uint32_t sessionID) = 0;
  virtual void removeSession(uint32_t sessionID) = 0;

  virtual void addParticipant(uint32_t sessionID, const QString& cname) = 0;
  virtual void removeParticipant(uint32_t sessionID, const QString& cname) = 0;

  // MEDIA
  // basic information on audio/video. Can be called in case information changes.
  virtual void audioInfo(uint32_t sessionID, uint32_t bitrate, uint32_t sampleRate, uint16_t channelCount) = 0;
  virtual void videoInfo(uint32_t sessionID, uint32_t bitrate, double framerate, QSize resolution) = 0;

  virtual void selectedICEPair(uint32_t sessionID, std::shared_ptr<ICEPair> pair) = 0;

  virtual void encodedAudioFrame(uint32_t size, uint32_t encodingTime) = 0;
  virtual void encodedVideoFrame(uint32_t size, uint32_t encodingTime, QSize resolution,
                                 float psnrY = -1.0, float psnrU = -1.0, float psnrV = -1.0) = 0;

  virtual void decodedAudioFrame(QString cname, uint32_t size, uint32_t decodingTime) = 0;
  virtual void decodedVideoFrame(QString cname, uint32_t size, uint32_t decodingTime, QSize resolution) = 0;

  virtual void audioLatency(uint32_t sessionID, QString cname, int64_t delay) = 0;
  virtual void videoLatency(uint32_t sessionID, QString cname, int64_t delay) = 0;

  // DELIVERY
  // Tracking of sent packets
  virtual void addSendPacket(uint32_t size) = 0;

  // tracking of received packets.
  virtual void addReceivePacket(uint32_t sessionID, const QString& cname, QString type, uint32_t size) = 0;

  // Details of an individual packet that shows how well our data is getting delivered
  virtual void addRTCPPacket(uint32_t sessionID, const QString& cname, QString type,
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
