#pragma once

#include "filter.h"

#include <QString>

class StatisticsInterface;
class ResourceAllocator;

class HybridFilter : public Filter
{
public:
  HybridFilter(QString id, StatisticsInterface *stats,
               std::shared_ptr<ResourceAllocator> hwResources);

  ~HybridFilter() override;

protected:
  virtual void process() override;

private:

  int count_;
};
