#include "hybridslavefilter.h"

HybridSlaveFilter::HybridSlaveFilter(QString id, StatisticsInterface *stats,
                                     std::shared_ptr<ResourceAllocator> hwResources, DataType type):
Filter(id, "Hybrid", stats, hwResources, type, type)
{}


void HybridSlaveFilter::setConnection(int index, bool status)
{
  setOutputStatus(index, status);
}

int HybridSlaveFilter::getBitrate()
{
  // TODO: get actual bit rate
  return 24000;
}


void HybridSlaveFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {

    sendOutput(std::move(input));
    input = getInput();
  }
}
