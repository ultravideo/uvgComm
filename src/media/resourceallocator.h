#pragma once

#include "processing/filter.h"

#include <QObject>
#include <qsize.h>

/* The purpose of this class is the enable filters to easily query the
 * state of hardware in terms of possible optimizations and performance. */

struct StreamInfo
{
  uint32_t previousJitter;
  int32_t  previousLost;

  int bitrate;
};


enum ArchitectureBitrate
{
  SINGLE_UPLINK_BITRATE,
  MULTI_UPLINK_BITRATE,
  HYBRID_UPLINK_BITRATE
};


class ResourceAllocator : public QObject
{
  Q_OBJECT
public:
  ResourceAllocator();

  // determines how we limit the maximum bitrate
  void setArchitectureBitrate(ArchitectureBitrate bitrate);

  void setParticipants(int otherParticipants);

  void updateSettings();

  bool isAVX2Enabled();
  bool isSSE41Enabled();

  bool useManualROI();
  bool useAutoROI();

  uint16_t getRoiObject() const;

  void addRTCPReport(uint32_t sessionID, DataType type,
                     int32_t lost, uint32_t jitter);

  void setConferenceBitrate(DataType type, int bitrate);
  int getEncoderBitrate(DataType type);

  void setConferenceResolution(const QSize& resolution);
  QSize getVideoResolution() const;

  uint8_t getRoiQp() const;
  uint8_t getBackgroundQp() const;

private:

  void updateGlobalBitrate(int& bitrate,
                           std::map<uint32_t, std::shared_ptr<StreamInfo> > &streams);

  std::shared_ptr<StreamInfo> getStreamInfo(uint32_t sessionID, DataType type);

  int conferenceBitratePortion(DataType type);

  void limitUploadBitrate(int &bitrate, DataType type);

  bool avx2_ = false;
  bool sse41_ = false;

  bool manualROI_ = false;
  bool autoROI_ = false;

  // key is sessionID
  std::map<uint32_t, std::shared_ptr<StreamInfo>> audioStreams_;
  std::map<uint32_t, std::shared_ptr<StreamInfo>> videoStreams_;

  QMutex bitrateMutex_;
  int conferenceVideoBitrate_;
  int conferenceAudioBitrate_;

  uint8_t roiQp_;
  uint8_t backgroundQp_;

  uint16_t roiObject_;

  int otherParticipants_;
  int visibleParticipants_;

  ArchitectureBitrate bitrateMode_;
  QString conferenceViewMode_;
  bool isSpeaker_;

  QSize conferenceResolution_;
  QSize videoResolution_;

  int uploadBandwidth_;

  int hybridPrioritization_;
};
