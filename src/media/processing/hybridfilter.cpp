#include "hybridfilter.h"

#include "logger.h"

HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO)
{}


HybridFilter::~HybridFilter()
{
  // Destructor implementation
}


void HybridFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // TODO: Select which outputs to disable
    Logger::getLogger()->printNormal("Hybrid", "process");
    sendOutput(std::move(input));
    input = getInput();
  }
}
