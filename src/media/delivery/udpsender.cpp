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
  if (destination.find(':') != std::string::npos)
  {
    dest_addr6_.sin6_family = AF_INET6;
    dest_addr6_.sin6_port = htons(port);
    inet_pton(AF_INET6, destination.c_str(), &dest_addr6_.sin6_addr);
  }
  else
  {
    dest_addr_.sin_family = AF_INET;
    dest_addr_.sin_port = htons(port);
    inet_pton(AF_INET, destination.c_str(), &dest_addr_.sin_addr);
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


bool UDPSender::lastFragment(std::unique_ptr<uchar[]>& data)
{
  return (data[1] >> 7) & 0x01;
}


void UDPSender::process()
{
  std::unique_ptr<Data> input = getInput();

  while (input)
  { 
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
  relay_->sendUDPData(destination_, port_, std::move(data), packetSize);
}
