#include "udpreceiver.h"

UDPReceiver::UDPReceiver(QString id, StatisticsInterface *stats,
                         std::shared_ptr<ResourceAllocator> hwResources):
    Filter("UDPReceiver", "UDPReceiver", stats, hwResources, DT_NONE, DT_RTP)
{
  maxBufferSize_ = 1000;
}


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
