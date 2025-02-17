#pragma once

#include "relayinterface.h"

#include <QUdpSocket>
#include <QObject>

#include <memory>
#include <string>
#include <unordered_map>

// The performance of this Qt implementation is not good enough for SFU

class Filter;

class UDPRelay : public QObject, public RelayInterface
{
public:
  UDPRelay(std::string localAddress, uint16_t port);

  virtual void registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter);

  virtual void sendUDPData(std::string destinationAddress, uint16_t port,
                           std::unique_ptr<unsigned char[]> data, uint32_t size);

  virtual void sendUDPData(sockaddr_in& dest_addr, sockaddr_in6& dest_addr6,
                           std::unique_ptr<unsigned char[]> data, uint32_t size);


  virtual void start() {}
  virtual void stop() {}

private slots:
  void readPendingDatagrams();

private:

  QUdpSocket socket_;

  std::unordered_map<uint32_t, std::shared_ptr<Filter>> receivers_;

};

