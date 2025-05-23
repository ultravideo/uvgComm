#include "hybridfilter.h"

#include "hybridslavefilter.h"

#include "logger.h"

HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO),
    triggerReEvaluation_(false)
{}


HybridFilter::~HybridFilter()
{
  cnameToLinks_.clear();
}


void HybridFilter::addLink(LinkType type,
                           uint32_t ssrc,
                           const QString& cname,
                           std::shared_ptr<UvgRTPSender> rtpSender)
{
  if (sizeOfOutputConnections() == 0)
  {
    Logger::getLogger()->printError("Hybrid", "No output connections available for link");
    return;
  }

  unsigned int outIdx = sizeOfOutputConnections() - 1;

  auto link = std::make_shared<LinkInfo>();
  link->type = type;
  link->active = false;
  link->ssrc = ssrc;
  link->outIndex = outIdx;
  link->rtpSender = rtpSender;

  // Register in cnameToLinks_
  LinkPair& pair = cnameToLinks_[cname];

  if (type == LINK_P2P)
  {
    pair.p2p = link;
    setOutputStatus(outIdx, false);
  }
  else if (type == LINK_SFU)
  {
    pair.sfu = link;
    link->active = true;
    setOutputStatus(outIdx, true);
  }
  else
  {
    Logger::getLogger()->printError("Hybrid", "Unknown link type");
    return;
  }

  triggerReEvaluation_ = true;
}


void HybridFilter::addSlave(std::shared_ptr<HybridSlaveFilter> slave)
{
  slaveMutex_.lock();
  slaves_.push_back(slave);
  slaveMutex_.unlock();
}


void HybridFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    sendOutput(std::move(input));
    input = getInput();
  }
}


void HybridFilter::setConnection(int index, bool status)
{
  setOutputStatus(index, status);

  slaveMutex_.lock();
  for (auto& slave : slaves_)
  {
    slave->setConnection(index, status);
  }
  slaveMutex_.unlock();
}
