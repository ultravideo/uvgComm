#include "udpreceiver.h"

UDPReceiver::UDPReceiver(QString id, StatisticsInterface *stats,
                         std::shared_ptr<ResourceAllocator> hwResources):
    Filter("UDPReceiver", "UDPReceiver", nullptr, nullptr, DT_NONE, DT_RTP)
{}


void UDPReceiver::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // send data to filter graph
    sendOutput(std::move(input));
    input = getInput();
  }
}
