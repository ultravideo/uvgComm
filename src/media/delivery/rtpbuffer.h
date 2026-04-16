#pragma once

#include "media/processing/filter.h"
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

  void emptyBuffer();

  uint32_t currentSSRC_ = 0;
  uint32_t currentRTPTimestamp_ = 0;
  bool timestampInitialized_ = false;

  std::vector<std::unique_ptr<Data>> buffer_;
};
