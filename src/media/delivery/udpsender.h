#pragma once

#include "media/processing/filter.h"

#include <QTimer>

#include <memory>

class RelayInterface;

class UDPSender : public Filter
{
public:
  UDPSender(QString id, StatisticsInterface *stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            std::string destination, int port, std::shared_ptr<RelayInterface> relay);

  ~UDPSender();

public slots:

  void keepLive();

protected:

  void process();

private:

  bool lastFragment(std::unique_ptr<uchar[]>& data);

  std::string destination_;
  int port_;
  std::shared_ptr<RelayInterface> relay_;

  QTimer keepLiveTimer_;

  sockaddr_in dest_addr_ = {};
  sockaddr_in6 dest_addr6_ = {};

  std::vector<std::unique_ptr<Data>> fragments_;
};

