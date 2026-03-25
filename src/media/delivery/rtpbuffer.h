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

  // Arrival-order tracking (for logging only)
  uint32_t lastArrivalTimestamp_ = 0;
  bool haveLastArrivalTimestamp_ = false;

  // Missing-frame handling: only enabled when we detect a timestamp gap.
  bool waitingForMissing_ = false;
  int64_t missingStartMs_ = -1;

  static constexpr int64_t MAX_MISSING_WAIT_MS = 300;

  // Fixed RTP timestamp delta for video frames at 30fps.
  static constexpr uint32_t FIXED_FRAME_DELTA_30FPS = (VIDEO_RTP_TIMESTAMP_RATE / 30);
  static constexpr uint32_t MIN_ACCEPTABLE_DELTA = (FIXED_FRAME_DELTA_30FPS / 2);           // -50%
  static constexpr uint32_t MAX_ACCEPTABLE_DELTA = (FIXED_FRAME_DELTA_30FPS + MIN_ACCEPTABLE_DELTA); // +50%
};
