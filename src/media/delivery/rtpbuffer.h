#pragma once

#include "media/processing/filter.h"
#include <deque>

class RTPBuffer : public Filter
{
public:
  RTPBuffer(QString id, StatisticsInterface* stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            DataType type);

protected:
  virtual void process() override;

private:
  std::deque<std::unique_ptr<Data>> reorderBuffer_;
  const size_t BUFFER_SIZE = 2; // 2-frame buffer for reordering
};
