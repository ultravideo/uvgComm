#pragma once

#include "media/processing/filter.h"
#include <deque>
#include <vector>

struct Frame
{
  uint32_t timestamp;
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
  const size_t BUFFER_SIZE = 3; // number of complete frames to buffer
};
