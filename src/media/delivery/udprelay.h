#pragma once

#include <QUdpSocket>
#include <QObject>

#include <memory>
#include <string>
#include <unordered_map>


class Filter;

class UDPRelay : public QObject
{
public:
  UDPRelay(std::string localAddress, uint16_t port);

  void registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter);

  void sendUDPData(std::string destinationAddress, uint16_t port,
                   std::unique_ptr<unsigned char[]> data, uint32_t size);

  private slots:
    void readPendingDatagrams();

private:

  QUdpSocket socket_;

  std::unordered_map<uint32_t, std::shared_ptr<Filter>> receivers_;

};

