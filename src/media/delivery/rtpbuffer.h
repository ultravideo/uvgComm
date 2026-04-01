#pragma once

#include "media/processing/filter.h"
#include <deque>
#include <vector>

struct Frame
{
  uint32_t timestamp;
  // SSRC for packets grouped in this frame (0 = unknown)
  uint32_t ssrc = 0;
  std::vector<std::unique_ptr<Data>> nalUnits;
};

class RTPBuffer : public Filter
{
public:
  RTPBuffer(QString id, StatisticsInterface* stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            DataType type);

protected:
  virtual void process() override;

private:
  void bufferFrame(std::unique_ptr<Data> packet);
  std::vector<Frame> collectFramesToOutput(int64_t nowMs);
  void setTimestamp(uint32_t outputRtpTimestamp);

  std::deque<Frame> frameBuffer_;

  // Output ordering state
  uint32_t lastOutputTimestamp_ = 0;
  bool haveLastOutputTimestamp_ = false;

  // Link switch handling: both links use different SSRCs.
  // We treat the first observed "other" SSRC as the new link and allow up to MAX_MISSING_WAIT_MS
  // for old-link tail frames to arrive before we give up and commit to the new link.
  uint32_t primarySsrc_ = 0;
  bool havePrimarySsrc_ = false;

  uint32_t secondarySsrc_ = 0;
  bool haveSecondarySsrc_ = false;

  bool switchingLinks_ = false;
  int64_t switchStartMs_ = -1;

  // Missing-frame handling: only enabled when we detect a timestamp gap.
  bool waitingForMissing_ = false;
  int64_t missingStartMs_ = -1;

  static constexpr int64_t MAX_MISSING_WAIT_MS = 400;

  // Fixed RTP timestamp delta for video frames at 30fps.
  static constexpr uint32_t FIXED_FRAME_DELTA_30FPS = (VIDEO_RTP_TIMESTAMP_RATE / 30);
  static constexpr uint32_t MIN_ACCEPTABLE_DELTA = (FIXED_FRAME_DELTA_30FPS / 2);           // -50%
  static constexpr uint32_t MAX_ACCEPTABLE_DELTA = (FIXED_FRAME_DELTA_30FPS + MIN_ACCEPTABLE_DELTA); // +50%
};
