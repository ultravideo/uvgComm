#include "uvgrelay.h"

#include "src/media/processing/filter.h"

#include "logger.h"


#include <QDateTime>
#include <cstring>

#ifndef _WIN32
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#else
#define MSG_DONTWAIT 0
#endif

const int POLL_TIMEOUT_MS = 200;

const int BUFFER_SIZE = 1500;

UVGRelay::UVGRelay(std::string localAddress, uint16_t port):
  socket_(0),
running_(false),
    ipv6_(false)
{
  // check if the local address is IPv4 or IPv6
  if (localAddress.find(':') != std::string::npos)
  {
    Logger::getLogger()->printNormal(this, "IPv6 address detected");
    socket_.init(AF_INET6, SOCK_DGRAM, 0);

    sockaddr_in6 local_addr;
    local_addr.sin6_family = AF_INET6;
    local_addr.sin6_port = htons(port);
    inet_pton(AF_INET6, localAddress.c_str(), &local_addr.sin6_addr);
    ipv6_ = true;
    socket_.bind_ip6(local_addr);

  }
  else
  {
    Logger::getLogger()->printNormal(this, "IPv4 address detected");
    socket_.init(AF_INET, SOCK_DGRAM, 0);
    ipv6_ = false;
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    inet_pton(AF_INET, localAddress.c_str(), &local_addr.sin_addr);

    socket_.bind(local_addr);
  }

  /* Set the default UDP send/recv buffer sizes to 4MB as on Windows
     * the default size is way too small for a larger video conference */
  int buf_size = 4 * 1024 * 1024;

  if (socket_.setsockopt(SOL_SOCKET, SO_SNDBUF, (const char*)&buf_size, sizeof(int)) != RTP_OK ||
      socket_.setsockopt(SOL_SOCKET, SO_RCVBUF, (const char*)&buf_size, sizeof(int)) != RTP_OK)
  {
    Logger::getLogger()->printError(this, "Failed to set the UDP buffer sizes");
  }
}


UVGRelay::~UVGRelay()
{}


void UVGRelay::registerRTPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter)
{
  rtpReceivers_[ssrc] = filter;
}


void UVGRelay::registerRTCPReceiver(uint32_t ssrc, std::shared_ptr<Filter> filter)
{
  rtcpReceivers_[ssrc] = filter;
}


void UVGRelay::sendUDPData(std::string destinationAddress, uint16_t port,
                           std::unique_ptr<unsigned char[]> data, uint32_t size)
{
  sockaddr_in dest_addr = {};
  sockaddr_in6 dest_addr6 = {};

  if (destinationAddress.find(':') != std::string::npos)
  {
    dest_addr6.sin6_family = AF_INET6;
    dest_addr6.sin6_port = htons(port);
    inet_pton(AF_INET6, destinationAddress.c_str(), &dest_addr6.sin6_addr);
  }
  else
  {
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, destinationAddress.c_str(), &dest_addr.sin_addr);
  }

  socket_.sendto(dest_addr, dest_addr6, data.get(), size, 0);
}


void UVGRelay::sendUDPData(sockaddr_in& dest_addr, sockaddr_in6& dest_addr6,
                           std::unique_ptr<unsigned char[]> data, uint32_t size)
{
  socket_.sendto(dest_addr, dest_addr6, data.get(), size, 0);
}

void UVGRelay::sendUDPData(sockaddr_in& dest_addr,
                           sockaddr_in6& dest_addr6,
                           std::vector<std::vector<std::pair<size_t, uint8_t*>>>& buffers)
{
  int bytes_sent = 0;
  socket_.__sendtov(dest_addr, dest_addr6, ipv6_, buffers, 0, &bytes_sent);
}


void UVGRelay::run()
{
  setPriority(QThread::TimeCriticalPriority);

   // poll from socket
#ifdef _WIN32
  LPWSAPOLLFD pfds = new pollfd();
#else
  pollfd* pfds = new pollfd();
#endif

  size_t read_fds = socket_.get_raw_socket();
  pfds->fd = read_fds;
  pfds->events = POLLIN;

 uint8_t buffer[BUFFER_SIZE];

  while (running_)
  {
    // poll for incoming data
#ifdef _WIN32
    if (WSAPoll(pfds, 1, POLL_TIMEOUT_MS) < 0) {
#else
    if (poll(pfds, 1, POLL_TIMEOUT_MS) < 0) {
#endif
      Logger::getLogger()->printError(this, "poll() failed");
      break;
    }

    if (pfds->revents & POLLIN)
    {
      while(running_)
      {
        int read = 0;
        rtp_error_t ret = socket_.recvfrom(buffer, BUFFER_SIZE, MSG_DONTWAIT, &read);

        if (ret == RTP_INTERRUPTED)
        {
          break;
        }
        else if (ret != RTP_OK)
        {
          Logger::getLogger()->printError(this, "recvfrom() failed");
          running_ = false;
          break;
        }

        if (read < 12)
        {
          //Logger::getLogger()->printNormal(this, "Received a packet which is too small to be an RTP packet",
          //                                {"Packet size"}, {QString::number(read)});
          continue;
        }

        // check if this is an RTP or RTCP packet
        uint8_t rtcp_pt = buffer[1];
        uint8_t rtp_pt = rtcp_pt & 0x7F;

        if (rtp_pt <= 34 || 96 <= rtp_pt)
        {
          // RTP
          uint32_t ssrc = 0;
          std::memcpy(&ssrc, buffer + 8, sizeof(ssrc));
          ssrc = ntohl(ssrc);

          if (rtpReceivers_.find(ssrc) != rtpReceivers_.end())
          {
            std::shared_ptr<Filter> filter = rtpReceivers_[ssrc];

            std::unique_ptr<Data> receivedRTPFrame = Filter::initializeData(DT_RTP, DS_REMOTE);
            receivedRTPFrame->creationTimestamp = QDateTime::currentMSecsSinceEpoch();
            receivedRTPFrame->presentationTimestamp = receivedRTPFrame->creationTimestamp;

            receivedRTPFrame->data = std::unique_ptr<uchar[]>(new uchar[read]);
            memcpy(receivedRTPFrame->data.get(), buffer, read);
            receivedRTPFrame->data_size = read;

            filter->putInput(std::move(receivedRTPFrame));
          }
          else
          {
            Logger::getLogger()->printWarning(this, "Received an RTP packet for which we have no receiver",
                                            {"SSRC"}, {QString::number(ssrc)});
          }
        }
        else if (rtcp_pt == 200 || rtcp_pt == 201 || rtcp_pt == 202)
        {
          handleRTCPCompound(buffer, read);

          // RTCP
          uint32_t ssrc = 0;
          std::memcpy(&ssrc, buffer + 4, sizeof(ssrc));
          ssrc = ntohl(ssrc);

          if (rtcpReceivers_.find(ssrc) != rtcpReceivers_.end())
          {
            std::shared_ptr<Filter> filter = rtcpReceivers_[ssrc];

            std::unique_ptr<Data> receivedRTPFrame = Filter::initializeData(DT_RTP, DS_REMOTE);
            receivedRTPFrame->creationTimestamp = QDateTime::currentMSecsSinceEpoch();
            receivedRTPFrame->presentationTimestamp = receivedRTPFrame->creationTimestamp;

            receivedRTPFrame->data = std::unique_ptr<uchar[]>(new uchar[read]);
            memcpy(receivedRTPFrame->data.get(), buffer, read);
            receivedRTPFrame->data_size = read;

            filter->putInput(std::move(receivedRTPFrame));
          }
          else
          {
            Logger::getLogger()->printWarning(this, "Received an RTCP packet for which we have no receiver",
                                            {"SSRC"}, {QString::number(ssrc)});
          }
        }
        else
        {
          Logger::getLogger()->printWarning(this, "Received a packet which does not follow RTP specifications",
                                          {"Payload type"},
                                          {QString::number(rtp_pt)});
        }
      }
    }
  }

  delete pfds;
}

void UVGRelay::handleRTCPCompound(const uint8_t* buffer, int length)
{
  int offset = 0;

  while (offset + 4 <= length)
  {
    const uint8_t firstOctet = buffer[offset];
    const uint8_t version = firstOctet >> 6;
    if (version != 2)
    {
      break;
    }

    uint16_t wordLength = 0;
    std::memcpy(&wordLength, buffer + offset + 2, sizeof(wordLength));
    wordLength = ntohs(wordLength);
    int packetSize = (static_cast<int>(wordLength) + 1) * 4;
    if (packetSize <= 0 || offset + packetSize > length)
    {
      break;
    }

    const uint8_t payloadType = buffer[offset + 1];
    if (payloadType == 204 && packetSize >= 12)
    {
      const uint8_t subtype = firstOctet & 0x1F;

      uint32_t senderSsrc = 0;
      std::memcpy(&senderSsrc, buffer + offset + 4, sizeof(senderSsrc));
      senderSsrc = ntohl(senderSsrc);

      char name[5];
      std::memcpy(name, buffer + offset + 8, 4);
      name[4] = '\0';
      QString appName = QString::fromLatin1(name, 4);

      if (packetSize >= 20)
      {
        uint32_t targetSsrc = 0;
        std::memcpy(&targetSsrc, buffer + offset + 12, sizeof(targetSsrc));
        targetSsrc = ntohl(targetSsrc);

        uint32_t timestamp = 0;
        if (packetSize >= 24)
        {
          std::memcpy(&timestamp, buffer + offset + 16, sizeof(timestamp));
          timestamp = ntohl(timestamp);

          emit rtcpAppPacketReceived(senderSsrc, targetSsrc, timestamp, appName, subtype);
        }
      }
    }

    offset += packetSize;
  }
}
