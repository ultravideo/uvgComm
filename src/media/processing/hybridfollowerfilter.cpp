#include "hybridfollowerfilter.h"

#include "media/resourceallocator.h"

HybridFollowerFilter::HybridFollowerFilter(QString id, StatisticsInterface *stats,
                                     std::shared_ptr<ResourceAllocator> hwResources, DataType type):
Filter(id, "Hybrid", stats, hwResources, type, type)
{}


void HybridFollowerFilter::setConnection(int index, bool status)
{
  setOutputStatus(index, status);
}


int HybridFollowerFilter::getBitrate()
{
  // Report the bitrate that is actually used by the pipeline.
  // For Opus, the encoder updates its bitrate from ResourceAllocator every frame,
  // so reading the raw setting here can overestimate and skew HybridFilter's
  // max-connection calculation.
  if (output_ == DT_RAWAUDIO)
    return 0;

  return getHWManager()->getEncoderBitrate(output_);
}


void HybridFollowerFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {

    sendOutput(std::move(input));
    input = getInput();
  }
}
