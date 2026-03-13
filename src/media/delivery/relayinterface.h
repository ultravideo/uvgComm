#pragma once

#include <string>
#include <memory>

#include <vector>

struct sockaddr_in;
struct sockaddr_in6;

class Filter;

class RelayInterface
{
public:
  virtual ~RelayInterface(){}

  virtual void start() = 0;
  virtual void stop() = 0;

  virtual bool isRunning()
  {
    return false;
  };

  virtual void registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter) = 0;
  virtual void registerRTCPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter) = 0;

  virtual void sendUDPData(std::string destinationAddress, uint16_t port,
                   std::unique_ptr<unsigned char[]> data, uint32_t size) = 0;

  virtual void sendUDPData(sockaddr_in &dest_addr,
                           sockaddr_in6 &dest_addr6,
                           std::unique_ptr<unsigned char[]> data,
                           uint32_t size) = 0;

  virtual void sendUDPData(sockaddr_in &dest_addr,
                           sockaddr_in6 &dest_addr6,
                           std::vector<std::vector<std::pair<size_t, uint8_t *>>>& buffers) = 0;
};
