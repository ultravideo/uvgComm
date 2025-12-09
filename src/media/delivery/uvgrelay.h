#pragma once

#include "relayinterface.h"
#include "uvgrtp_socket.hh"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include <QThread>
#include <QString>

class Filter;


class UVGRelay : public QThread, public RelayInterface
{
  Q_OBJECT
public:
  UVGRelay(std::string localAddress, uint16_t port);
  ~UVGRelay();

  virtual void registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter);
  virtual void registerRTCPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter);

  virtual void sendUDPData(std::string destinationAddress, uint16_t port,
                           std::unique_ptr<unsigned char[]> data, uint32_t size);

  virtual void sendUDPData(sockaddr_in &dest_addr,
                   sockaddr_in6 &dest_addr6,
                   std::unique_ptr<unsigned char[]> data,
                   uint32_t size);


  virtual void sendUDPData(sockaddr_in &dest_addr,
                           sockaddr_in6 &dest_addr6,
                           std::vector<std::vector<std::pair<size_t, uint8_t *>>>& buffers);

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

signals:
  void rtcpAppPacketReceived(uint32_t senderSsrc,
                             uint32_t targetSsrc,
                             uint32_t rtpTimestamp,
                             QString appName,
                             uint8_t subtype);

private:

  struct ReceivedPacket {
    uint8_t buffer[1500];
    int size;
  };

  void handleRTCPCompound(const uint8_t* buffer, int length);
  void processPackets(); // Worker thread function
  void processReceivedPacket(const uint8_t* buffer, int size);

  std::unordered_map<uint32_t, std::shared_ptr<Filter>> rtpReceivers_;
  std::unordered_map<uint32_t, std::shared_ptr<Filter>> rtcpReceivers_;

  // this class is stolen from uvgRTP
  uvgrtp::socket socket_;

  bool running_;
  bool ipv6_;

  // Packet queue for decoupling reception from processing
  std::queue<ReceivedPacket> packetQueue_;
  std::mutex queueMutex_;
  std::condition_variable queueCV_;
  std::thread processingThread_;
  std::atomic<bool> processingRunning_;
};
