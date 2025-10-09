#pragma once

#include "statisticsinterface.h"

#include <QStringList>

#include <unordered_map>
#include <vector>

class StatisticsCSV : public StatisticsInterface
{
public:
  StatisticsCSV(QString folder, QString sipLogFile);

  virtual void addSession(uint32_t sessionID) override;

  // writes the csv
  virtual void removeSession(uint32_t sessionID) override;

  virtual void addParticipant(uint32_t sessionID, const QString& cname) override;

  // writes the participant
  virtual void removeParticipant(uint32_t sessionID, const QString& cname) override;

  // ignored
  virtual void audioInfo(uint32_t sessionID, uint32_t bitrate, uint32_t sampleRate, uint16_t channelCount) override;
  virtual void videoInfo(uint32_t sessionID, uint32_t bitrate, double framerate, QSize resolution) override;
  virtual void selectedICEPair(uint32_t sessionID, std::shared_ptr<ICEPair> pair) override;
  virtual void addRTCPPacket(uint32_t sessionID, const QString& cname, QString type,
                             uint8_t fraction, int32_t lost, uint32_t last_seq,
                             uint32_t jitter) override;

  // record data for the csv file.
  virtual void audioLatency(uint32_t sessionID, QString cname, int64_t timestamp, int64_t delay) override;
  virtual void videoLatency(uint32_t sessionID, QString cname, int64_t timestamp, int64_t delay) override;
  virtual void encodedAudioFrame(uint32_t size, uint32_t encodingTime) override;
  virtual void encodedVideoFrame(uint32_t size,
                                 uint32_t encodingTime,
                                 QSize resolution,
                                 float psnrY = -1.0,
                                 float psnrU = -1.0,
                                 float psnrV = -1.0,
                                 int64_t creationTimestamp = 0.0) override;
  virtual void decodedAudioFrame(QString cname, int64_t timestamp, uint32_t size, uint32_t decodingTime) override;
  virtual void decodedVideoFrame(QString cname, int64_t timestamp, uint32_t size, uint32_t decodingTime, QSize resolution) override;

  // ignored
  virtual void addSendPacket(uint32_t size) override;
  virtual void addReceivePacket(uint32_t sessionID, const QString& cname, QString type, uint32_t size) override;


  // FILTER
  // tell the that we want to track this filter or stop tracking
  virtual uint32_t addFilter(QString type, QString identifier, uint64_t TID) override;
  virtual void removeFilter(uint32_t id) override;

  // Tracking of buffer information.
  virtual void updateBufferStatus(uint32_t id, uint16_t buffersize,
                                  uint16_t maxBufferSize) override;

  // Tracking of packets dropped due to buffer overflow
  virtual void packetDropped(uint32_t id) override;

  // SIP
  // Tracking of sent and received SIP Messages
  virtual void addSentSIPMessage(const QString& headerType, const QString& header,
                                 const QString& bodyType,   const QString& body) override;
  virtual void addReceivedSIPMessage(const QString& headerType, const QString& header,
                                     const QString& bodyType,   const QString& body) override;

private:

  struct EncodedFrame
  {
    uint32_t size;
    uint32_t encodingTime;

    QSize resolution;
    float psnrY = -1.0f;
    float psnrU = -1.0f;
    float psnrV = -1.0f;

    int64_t creationTimestamp = -1;
  };

  struct DecodedFrame
  {
    uint32_t size;
    uint32_t decodingTime;

    QSize resolution;

    int64_t timestamp = -1;
  };

  struct LocalInfo
  {
    std::vector<EncodedFrame> encodedAudioFrames;
    std::vector<EncodedFrame> encodedVideoFrames;
  };

  LocalInfo localInfo_;

  struct SessionInfo
  {
    std::unordered_map<int64_t, int64_t> audioLatencies;
    std::unordered_map<int64_t, int64_t> videoLatencies;
    std::vector<DecodedFrame> decodedAudioFrames;
    std::vector<DecodedFrame> decodedVideoFrames;
  };

  std::unordered_map<QString, SessionInfo> sessionInfo_;
  std::unordered_map<uint32_t, QStringList> sessionNames_;

  QString folder_;
  QString sipLogFile_;
};
