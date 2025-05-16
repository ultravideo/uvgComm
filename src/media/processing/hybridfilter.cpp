#include "hybridfilter.h"

#include "logger.h"

HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO)
{
  count_ = 0;
}


HybridFilter::~HybridFilter()
{
  // Destructor implementation
}


void HybridFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    ++count_;
    Logger::getLogger()->printNormal("Hybrid", "process");

    // TODO: Not correct as P2P connections get added one after another in conference call
    if (count_ % 320 == 0)
    {
      int phase = (count_ / 320) % 2;
      Logger::getLogger()->printNormal("Hybrid", "Switching mode at count: " + QString::number(count_));
      for (int i = 0; i < sizeOfOutputConnections(); i += 2)
      {
        setOutputStatus(i, phase == 0);
        setOutputStatus(i + 1, phase == 1);
      }
    }

    sendOutput(std::move(input));
    input = getInput();
  }
}
