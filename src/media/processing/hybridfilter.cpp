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


void HybridFilter::addLink(LinkType type)
{
  if (sizeOfOutputConnections() == 0)
  {
    Logger::getLogger()->printError("Hybrid", "No output connections available for link");
    return;
  }

  if (type == LINK_P2P)
  {
    p2pLinks_.push_back(sizeOfOutputConnections() - 1);
  }
  else if (type == LINK_SFU)
  {
    sfuLink_ = sizeOfOutputConnections() - 1;
  }
  else
  {
    Logger::getLogger()->printError("Hybrid", "Unknown link type");
  }
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

      // switch state of p2p links
      for (auto& link : p2pLinks_)
      {
        setOutputStatus(link, phase == 0);
      }

      // switch state of sfu link
      setOutputStatus(sfuLink_, phase == 1);
    }

    sendOutput(std::move(input));
    input = getInput();
  }
}
