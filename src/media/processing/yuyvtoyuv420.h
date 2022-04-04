#pragma once

#include "filter.h"

class YUYVtoYUV420 : public Filter
{
public:
  YUYVtoYUV420(QString id, StatisticsInterface *stats,
               std::shared_ptr<ResourceAllocator> hwResources);

protected:
  void process();
};
