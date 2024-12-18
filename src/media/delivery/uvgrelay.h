#pragma once

#include "relayinterface.h"
#include "uvgrtp_socket.hh"

#include <memory>
#include <string>
#include <unordered_map>

#include <QThread>

class UVGRelay : public RelayInterface, public QThread
{
public:
  UVGRelay(std::string localAddress, uint16_t port);
  ~UVGRelay();

  virtual void registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter);

  virtual void sendUDPData(std::string destinationAddress, uint16_t port,
                           std::unique_ptr<unsigned char[]> data, uint32_t size);

  virtual void run();

  virtual void start()
  {
    running_ = true;
    QThread::start();
  }

  virtual void stop()
  {
    QThread::quit();
    running_ = false;
  }

  virtual bool isRunning()
  {
    return QThread::isRunning();
  }

private:

  std::unordered_map<uint32_t, std::shared_ptr<Filter>> receivers_;

  // this class is stolen from uvgRTP
  uvgrtp::socket socket_;

  bool running_;
};
