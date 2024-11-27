#pragma once

#include "media/processing/filter.h"

#include <memory>

class UDPRelay;

class UDPSender : public Filter
{
public:
  UDPSender(QString id, StatisticsInterface *stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            std::string destination, int port, std::shared_ptr<UDPRelay> relay);

protected:

  void process();

private:

  std::string destination_;
  int port_;
  std::shared_ptr<UDPRelay> relay_;

};

