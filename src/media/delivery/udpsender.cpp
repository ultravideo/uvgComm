#include "udpsender.h"

#include "media/delivery/relayinterface.h"

#include "logger.h"

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
  maxBufferSize_ = 1000;
  keepLiveTimer_.setInterval(5000);

  connect(&keepLiveTimer_, &QTimer::timeout, this, &UDPSender::keepLive);
  keepLiveTimer_.start();

  keepLive(); // opens the firewall
}


void UDPSender::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // send data over the network
    relay_->sendUDPData(destination_, port_, std::move(input->data), input->data_size);
    input = getInput();
  }
}


void UDPSender::keepLive()
{
  //Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Sending keep alive packet",
  //                                {"Destination"}, {QString::fromStdString(destination_) + ":" + QString::number(port_)});
  // send keep alive packet
  int packetSize = 2;
  std::unique_ptr<unsigned char[]> data(new unsigned char[packetSize]);
  data[0] = 0;
  data[1] = 0;
  relay_->sendUDPData(destination_, port_, std::move(data), packetSize);
}
