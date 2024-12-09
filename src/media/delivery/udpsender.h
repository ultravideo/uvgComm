#pragma once

#include "media/processing/filter.h"

#include <QTimer>

#include <memory>

class UDPRelay;

class UDPSender : public Filter
{
public:
  UDPSender(QString id, StatisticsInterface *stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            std::string destination, int port, std::shared_ptr<UDPRelay> relay);


public slots:

  void keepLive();

protected:

  void process();



private:

  std::string destination_;
  int port_;
  std::shared_ptr<UDPRelay> relay_;

  QTimer keepLiveTimer_;
};

