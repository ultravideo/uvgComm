#include "hybridfilter.h"

#include "hybridslavefilter.h"
#include "media/delivery/uvgrtpsender.h"
#include "media/resourceallocator.h"

#include "logger.h"

const uint8_t MAX_RTT_MEASUREMENTS = 64; // Maximum number of RTT samples to keep


HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO),
    triggerReEvaluation_(false),
    sfuIndex_(0),
    count_(0)
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
  // TODO: should we not only have one SFU link?


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
    if (pair.p2p)
    {
      Logger::getLogger()->printNormal("Hybrid", "P2P link already exists for cname: " + cname);
    }

    pair.p2p = link;
    setOutputStatus(outIdx, true);
  }
  else if (type == LINK_SFU)
  {
    if (pair.sfu)
    {
      Logger::getLogger()->printNormal("Hybrid", "SFU link already exists for cname: " + cname);
    }
    pair.sfu = link;
    link->active = true;
    setOutputStatus(outIdx, true);
    sfuIndex_ = outIdx;
  }
  else
  {
    Logger::getLogger()->printError("Hybrid", "Unknown link type");
    return;
  }

  QObject::connect(rtpSender.get(), &UvgRTPSender::rttReceived,
                   this, &HybridFilter::recordRTT);

  triggerReEvaluation_ = true;
}


void HybridFilter::recordRTT(uint32_t ssrc, double rtt)
{
  bool foundSSRC = false;

  for (auto& pair : cnameToLinks_)
  {
    if (pair.second.p2p && pair.second.p2p->ssrc == ssrc)
    {
      pair.second.p2p->rttSamples.push_back(rtt);
      if (pair.second.p2p->rttSamples.size() > MAX_RTT_MEASUREMENTS)
      {
        pair.second.p2p->rttSamples.pop_front();
      }
      foundSSRC = true;
      break;
    }
    if (pair.second.sfu && pair.second.sfu->ssrc == ssrc)
    {
      pair.second.sfu->rttSamples.push_back(rtt);
      if (pair.second.sfu->rttSamples.size() > MAX_RTT_MEASUREMENTS)
      {
        pair.second.sfu->rttSamples.pop_front();
      }

      foundSSRC = true;
      break;
    }
  }

  if (!foundSSRC)
  {
    Logger::getLogger()->printError("Hybrid", "RTT record for unknown SSRC: " + QString::number(ssrc));
    return;
  }
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
    //Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Processing input",
    //                                 {"Input size"},
    //                                 {QString::number(input->data_size)});

    ++count_;
    if (count_ % 640 == 0)
    {
      triggerReEvaluation_ = true;
    }

    if (triggerReEvaluation_)
    {
      reEvaluateConnections();
      triggerReEvaluation_ = false;
    }

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


double HybridFilter::averageRTT(const std::shared_ptr<LinkInfo>& link) const
{
  if (!link || link->rttSamples.empty())
  {
    return 0.0;
  }

  double sum = 0.0;
  for (const auto& sample : link->rttSamples)
  {
    sum += sample;
  }

  return sum / link->rttSamples.size();
}


void HybridFilter::reEvaluateConnections()
{
  const int networkBandwidth = 50000000; // TODO: get from somewhere
  const int connectionBandwidth = getHWManager()->getBitrate(output_)/cnameToLinks_.size();
  const int maxP2PConnections = networkBandwidth / connectionBandwidth;


  // calculate the average RTT for each link
  for (auto& pair : cnameToLinks_)
  {
    if (pair.second.p2p)
    {
      pair.second.p2p->latestsRtt = averageRTT(pair.second.p2p);
    }
    if (pair.second.sfu)
    {
      pair.second.sfu->latestsRtt = averageRTT(pair.second.sfu);
    }
  }

  // just choose the best connection if we have the bandwidth, usually P2P
  if (maxP2PConnections >= cnameToLinks_.size() + 1)
  {
    bool needSFU = false;
    for (auto& pair : cnameToLinks_)
    {
      if (pair.second.p2p && pair.second.sfu)
      {
        if (pair.second.p2p->latestsRtt < pair.second.sfu->latestsRtt) // use p2p
        {
          setConnection(pair.second.p2p->outIndex, true);
        }
        else // use SFU because it is faster for some reason
        {
          // this can happen only if P2P connection takes a worse route than the route via SFU would be which is rare
          Logger::getLogger()->printWarning(this, "SFU connection is faster for some reason that P2P");
          setConnection(pair.second.p2p->outIndex, false);
          needSFU = true;
        }
      }
    }

    // disables the sfu connection if we don't need it
    setConnection(sfuIndex_, needSFU);

    return;
  }

  // Otherwise, use P2P where RTT benefit is biggest

  struct RankedConnection
  {
    std::shared_ptr<LinkInfo> link;
    double rttBenefit; // lower is better
  };

  std::vector<RankedConnection> rankedConnections;

  for (auto& pair : cnameToLinks_)
  {
    if (pair.second.p2p && pair.second.sfu)
    {
      RankedConnection ranked;
      ranked.link = pair.second.p2p;
      ranked.rttBenefit = pair.second.sfu->latestsRtt - pair.second.p2p->latestsRtt;
      rankedConnections.push_back(ranked);
    }
  }

  // Sort connections by RTT benefit
  std::sort(rankedConnections.begin(), rankedConnections.end(),
            [](const RankedConnection& a, const RankedConnection& b) {
              return a.rttBenefit < b.rttBenefit;
            });


  // Enable P2P connections until we reach the limit
  for (size_t i = 0; i < rankedConnections.size(); ++i)
  {
    if (i < maxP2PConnections)
    {
      setConnection(rankedConnections[i].link->outIndex, true);
    }
    else
    {
      setConnection(rankedConnections[i].link->outIndex, false);
    }
  }
}
