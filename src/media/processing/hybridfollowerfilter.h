#pragma once

#include "filter.h"

class HybridFollowerFilter : public Filter
{
public:
  HybridFollowerFilter(QString id, StatisticsInterface *stats,
                    std::shared_ptr<ResourceAllocator> hwResources,
                    DataType type);

  ~HybridFollowerFilter() override = default;

  int getBitrate();

  void setConnection(int index, bool status);

protected:
  void process() override;
};
