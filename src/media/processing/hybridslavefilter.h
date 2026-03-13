#pragma once

#include "filter.h"

class HybridSlaveFilter : public Filter
{
public:
  HybridSlaveFilter(QString id, StatisticsInterface *stats,
                    std::shared_ptr<ResourceAllocator> hwResources,
                    DataType type);

  ~HybridSlaveFilter() override = default;

  int getBitrate();

  void setConnection(int index, bool status);

protected:
  void process() override;
};
