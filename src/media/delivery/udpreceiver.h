#pragma once

#include "media/processing/filter.h"

class UDPReceiver : public Filter
{
public:
  UDPReceiver(QString id, StatisticsInterface *stats,
              std::shared_ptr<ResourceAllocator> hwResources);

protected:

  void process();

};
