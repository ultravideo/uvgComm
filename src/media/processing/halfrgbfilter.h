#pragma once

#include "filter.h"

class HalfRGBFilter : public Filter
{
public:
  HalfRGBFilter(QString id, StatisticsInterface* stats,
                std::shared_ptr<ResourceAllocator> hwResources);

  virtual void updateSettings();

protected:

  void process();
};
