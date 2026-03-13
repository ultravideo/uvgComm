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
  std::deque<Frame> frameBuffer_;
  const size_t BUFFER_SIZE = 1; // number of complete frames to buffer

  // SSRC/state tracking for source-aware hold-mode
  uint32_t currentOutputSSRC_ = 0; // SSRC currently being output
  uint32_t pendingNewSSRC_ = 0; // SSRC that triggered hold-mode
  uint32_t pendingSwitchTimestamp_ = 0; // first timestamp observed from new SSRC
  // highest timestamp seen so far for the old/current output SSRC while in hold-mode
  uint32_t pendingOldMaxTimestamp_ = 0;
  bool holdMode_ = false; // when true, buffer outputs until condition met

  // Estimated RTP timestamp delta per frame (90kHz / 30fps = 3000). Used to compute thresholds.
  uint32_t estimatedFrameDelta_ = (VIDEO_RTP_TIMESTAMP_RATE / 30);
};
