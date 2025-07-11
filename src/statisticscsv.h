#pragma once

#include "statisticsinterface.h"

#include <unordered_map>
#include <vector>

class StatisticsCSV : public StatisticsInterface
{
public:
  StatisticsCSV();


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
  void addRTCPPacket(uint32_t sessionID, const QString& cname, QString type,
                     uint8_t fraction, int32_t lost, uint32_t last_seq,
                     uint32_t jitter) override;

  // record data for the csv file.
  void audioLatency(uint32_t sessionID, QString cname, int64_t delay) override;
  void videoLatency(uint32_t sessionID, QString cname, int64_t delay) override;
  void encodedAudioFrame(uint32_t size, uint32_t encodingTime) override;
  void encodedVideoFrame(uint32_t size, uint32_t encodingTime, QSize resolution,
                         float psnrY = -1.0, float psnrU = -1.0, float psnrV = -1.0) override;
  void decodedAudioFrame(QString cname, uint32_t size, uint32_t decodingTime) override;
  void decodedVideoFrame(QString cname, uint32_t size, uint32_t decodingTime, QSize resolution) override;

  // ignored
  void addSendPacket(uint32_t size) override;
  void addReceivePacket(uint32_t sessionID, const QString& cname, QString type, uint32_t size) override;

private:

  struct EncodedFrame
  {
    uint32_t size;
    uint32_t encodingTime;

    QSize resolution;
    float psnrY = -1.0f;
    float psnrU = -1.0f;
    float psnrV = -1.0f;
  };

  struct DecodedFrame
  {
    uint32_t size;
    uint32_t decodingTime;

    QSize resolution;
  };

  struct LocalInfo
  {
    std::vector<EncodedFrame> encodedAudioFrames;
    std::vector<EncodedFrame> encodedVideoFrames;
  };

  LocalInfo localInfo_;

  struct SessionInfo
  {
    std::vector<int64_t> audioLatencies;
    std::vector<int64_t> videoLatencies;
    std::vector<DecodedFrame> decodedAudioFrames;
    std::vector<DecodedFrame> decodedVideoFrames;
  };

  std::unordered_map<QString, SessionInfo> sessionInfo_;
  std::unordered_map<uint32_t, QStringList> sessionNames_;
};
