#include "hybridslavefilter.h"

#include "common.h"
#include "media/resourceallocator.h"
#include "settingskeys.h"

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
  // Report the bitrate that is actually used by the pipeline.
  // For Opus, the encoder updates its bitrate from ResourceAllocator every frame,
  // so reading the raw setting here can overestimate and skew HybridFilter's
  // max-connection calculation.
  if (output_ == DT_RAWAUDIO)
    return 0;

  return getHWManager()->getEncoderBitrate(output_);
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
