#include "udpsender.h"

#include "media/delivery/relayinterface.h"

#include "logger.h"

#include <QDateTime>

#ifdef _WIN32
#include <Windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif


UDPSender::UDPSender(QString id,
                     StatisticsInterface *stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     std::string destination,
                     int port,
                     std::shared_ptr<RelayInterface> relay)
    : Filter("UDPSender", "UDPSender", stats, hwResources, DT_RTP, DT_NONE)
    , destination_(destination)
    , port_(port)
    , relay_(relay)
    , keepLiveTimer_(this)
{
  // initialize atomic destination
  bool isIpv6 = (destination.find(':') != std::string::npos);
  {
    std::shared_ptr<Destination> initPtr = std::make_shared<Destination>(destination, port, isIpv6);
    std::atomic_store(&destPtr_, initPtr);
  }


  maxBufferSize_ = 1000;
  keepLiveTimer_.setInterval(5000);

  connect(&keepLiveTimer_, &QTimer::timeout, this, &UDPSender::keepLive);
  keepLiveTimer_.start();

  keepLive(); // opens the firewall
}


UDPSender::~UDPSender()
{
  keepLiveTimer_.stop();
}

void UDPSender::updateDestination(const std::string &destination, int port, bool ipv6)
{
  // atomically replace destination object
  std::shared_ptr<Destination> newPtr = std::make_shared<Destination>(destination, port, ipv6);
  std::atomic_store(&destPtr_, newPtr);
}


bool UDPSender::lastFragment(std::unique_ptr<uchar[]>& data)
{
  return (data[1] >> 7) & 0x01;
}


void UDPSender::process()
{
  std::unique_ptr<Data> input = getInput();

  while (input)
  { 
    /*
    TODO: uvgRTP does not always set marker bit correctly
    if (fragments_.empty() && lastFragment(input->data))
    {
      // no need to fragment
      relay_->sendUDPData(dest_addr_, dest_addr6_, std::move(input->data), input->data_size);
    }
    else if (lastFragment(input->data))
    {
      fragments_.push_back(std::move(input));

      std::vector<std::vector<std::pair<size_t, uint8_t *>>> fragments;

      for(auto& fragment : fragments_)
      {
        std::vector<std::pair<size_t, uint8_t *>> frag;
        frag.push_back(std::make_pair(fragment->data_size, fragment->data.get()));
        fragments.push_back(frag);
      }

      // send all fragments
      relay_->sendUDPData(dest_addr_, dest_addr6_, fragments);
      fragments_.clear();
    }
    else
    {
      fragments_.push_back(std::move(input));
    }
    */

    // delete if above optimization is enabled

    // send using string+port API to avoid accessing sockaddr structs without locks
    auto dest = std::atomic_load(&destPtr_);
    if (dest)
    {
      relay_->sendUDPData(dest->address, (uint16_t)dest->port, std::move(input->data), input->data_size);
    }

    input = getInput();
  }
}


void UDPSender::keepLive()
{
  //Logger::getLogger()->printNormal(this, "Sending keep alive packet",
  //                                {"Destination"}, {QString::fromStdString(destination_) + ":" + QString::number(port_)});
  // send keep alive packet
  int packetSize = 2;
  std::unique_ptr<unsigned char[]> data(new unsigned char[packetSize]);
  data[0] = 0;
  data[1] = 0;
  auto dest = std::atomic_load(&destPtr_);
  if (dest)
    relay_->sendUDPData(dest->address, (uint16_t)dest->port, std::move(data), packetSize);
}
