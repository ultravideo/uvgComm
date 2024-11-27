#include "udpsender.h"
#include "media/delivery/udprelay.h"

UDPSender::UDPSender(QString id, StatisticsInterface *stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     std::string destination, int port, std::shared_ptr<UDPRelay> relay):
    Filter("UDPSender", "UDPSender", nullptr, nullptr, DT_RTP, DT_NONE),
    destination_(destination),
    port_(port),
    relay_(relay)
{}


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
