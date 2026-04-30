#pragma once

#include "media/processing/filter.h"

#include <QTimer>

#include <memory>
#include <mutex>
#include <atomic>

class RelayInterface;

class UDPSender : public Filter
{
public:
  UDPSender(QString id, StatisticsInterface *stats,
            std::shared_ptr<ResourceAllocator> hwResources,
            std::string destination, int port, std::shared_ptr<RelayInterface> relay);

  ~UDPSender();

  // Update the destination address/port used when sending packets
  void updateDestination(const std::string &destination, int port, bool ipv6);

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
  // Atomic destination pointer to avoid mutexes while updating destination
  struct Destination
  {
    std::string address;
    int port;
    bool ipv6;
    Destination(const std::string &a, int p, bool v) : address(a), port(p), ipv6(v) {}
  };

  // Use plain shared_ptr with atomic_load/store for portability
  std::shared_ptr<Destination> destPtr_ { nullptr };

  std::vector<std::unique_ptr<Data>> fragments_;
};

