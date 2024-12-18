#pragma once

#include <string>
#include <memory>


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

  virtual void sendUDPData(std::string destinationAddress, uint16_t port,
                   std::unique_ptr<unsigned char[]> data, uint32_t size) = 0;
};
