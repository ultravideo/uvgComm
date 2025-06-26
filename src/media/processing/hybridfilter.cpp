#include "hybridfilter.h"

#include "hybridslavefilter.h"
#include "media/delivery/uvgrtpsender.h"
#include "media/resourceallocator.h"

#include "logger.h"

#include "settingskeys.h"
#include "common.h"

const uint8_t MAX_RTT_MEASUREMENTS = 64; // Maximum number of RTT samples to keep
const int EVALUATION_INTERVAL = 640; // Number of frames between evaluations


HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO),
    triggerReEvaluation_(false),
    sfuIndex_(-1),
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
    pair.p2p->active = false; // we use SFU only at the start
    setOutputStatus(outIdx, false);
  }
  else if (type == LINK_SFU)
  {
    if (pair.sfu)
    {
      Logger::getLogger()->printNormal("Hybrid", "SFU link already exists for cname: " + cname);
    }
    pair.sfu = link;
    pair.sfu->active = true; // SFU link is always active at the start
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
    if (count_ % EVALUATION_INTERVAL == 0)
    {
      triggerReEvaluation_ = true;
    }

    if (triggerReEvaluation_)
    {
      reEvaluateConnections();
      triggerReEvaluation_ = false;
      sendDummies(); // send dummies to get latency measurements for inactive connections
    }

    sendOutput(std::move(input));
    input = getInput();

    if (nextSwitch_ > 0)
    {
      --nextSwitch_;
      if (nextSwitch_ == 0)
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Switching connections",
                                        {"Next switch"}, {QString::number(nextSwitch_)});

        // Switch connections
        for (const auto& linkPair : linksToSwitch_)
        {
          if ((linkPair.p2p->active && linkPair.sfu->active) ||
              (!linkPair.p2p->active && !linkPair.sfu->active))
          {
            Logger::getLogger()->printError(this, "Invalid state during switch");
            continue; // both links are either active or inactive, cannot switch
          }
          if (linkPair.p2p->active && !linkPair.sfu->active)
          {
            linkPair.p2p->active = false;
            linkPair.sfu->active = true;
            setConnection(linkPair.p2p->outIndex, false);
          }
          else if (!linkPair.p2p->active && linkPair.sfu->active)
          {
            linkPair.sfu->active = false;
            linkPair.p2p->active = true;
            setConnection(linkPair.p2p->outIndex, true);
          }
        }
        linksToSwitch_.clear();
      }
    }
  }
}


void HybridFilter::sendDummies()
{
  // the point of dummy is to send a harmless packet via all inactive media paths so they send RTCP SR
  uint8_t dummy_packet[] = {
      0x00, 0x00, 0x00, 0x01, // 4-byte start code
      0x27,                   // NAL unit type 39 (SEI, non-IDR)
      0x80                    // Dummy SEI payload (incomplete, harmless)
  };

  std::unique_ptr<Data> newDummy = initializeData(output_, DS_LOCAL);
  auto now = std::chrono::system_clock::now();
  newDummy->creationTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  newDummy->presentationTimestamp = newDummy->creationTimestamp;

  newDummy->type = output_;
  newDummy->data_size = sizeof(dummy_packet);
  newDummy->data = std::unique_ptr<uchar[]>(new uchar[sizeof(dummy_packet)]);
  memcpy(newDummy->data.get(), dummy_packet, sizeof(dummy_packet));
  sendOutput(std::move(newDummy), true); // inverse = true to send data to inactive paths
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
  const int networkBandwidth = settingValue(SettingsKey::sipUpBandwidth);
  int connectionBandwidth = settingValue(SettingsKey::videoBitrate)/cnameToLinks_.size();

  for (auto& slave : slaves_)
  {
    connectionBandwidth += slave->getBitrate();;
  }

  connectionBandwidth = connectionBandwidth/0.95; // reserve 5% for overhead

  if (connectionBandwidth <= 0)
  {
    Logger::getLogger()->printError(this, "Connection bandwidth is zero or negative, cannot re-evaluate connections");
    return;
  }
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

  // we can do all connections however we like
  if (maxP2PConnections >= cnameToLinks_.size())
  {
    return fullBandwidthEvaluation();
  }

  // Otherwise, use P2P where RTT benefit is biggest

  struct RankedConnection
  {
    std::shared_ptr<LinkInfo> p2plink;
    std::shared_ptr<LinkInfo> sfulink;
    double rttBenefit; // higher is better
  };

  std::vector<RankedConnection> rankedConnections;

  for (auto& pair : cnameToLinks_)
  {
    if (pair.second.p2p && pair.second.sfu)
    {
      RankedConnection ranked;
      ranked.p2plink = pair.second.p2p;
      ranked.sfulink = pair.second.sfu;
      ranked.rttBenefit = pair.second.sfu->latestsRtt - pair.second.p2p->latestsRtt;
      rankedConnections.push_back(ranked);
    }
  }

  // Sort connections by RTT benefit
  std::sort(rankedConnections.begin(), rankedConnections.end(),
            [](const RankedConnection& a, const RankedConnection& b) {
              return a.rttBenefit < b.rttBenefit;
            });

  int p2pLinks = 0;
  // Enable P2P connections until we reach the limit
  for (size_t i = 0; i < rankedConnections.size(); ++i)
  {
    if (i < maxP2PConnections && !rankedConnections[i].p2plink->active && rankedConnections[i].rttBenefit > 0)
    {
      // this connection we can afford and most benefits from the P2P connection
      delayedSwitchToP2P(rankedConnections[i].p2plink, rankedConnections[i].sfulink);
      ++p2pLinks;
    }
    else if (rankedConnections[i].p2plink->active)
    {
      delayedSwitchToSFU(rankedConnections[i].p2plink, rankedConnections[i].sfulink);
    }
    else
    {
      ++p2pLinks;
    }
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Partial-bandwidth evaluation",
                                  {"Connection bandwidth", "maxP2PConnections", "P2P Links"},
                                  {QString::number(connectionBandwidth), QString::number(maxP2PConnections),
                                  QString::number(p2pLinks) + "/" + QString::number(cnameToLinks_.size())});
}


void HybridFilter::delayedSwitchToP2P(std::shared_ptr<LinkInfo> p2p, std::shared_ptr<LinkInfo> sfu)
{
  if (cnameToLinks_.size() == 1 || (!sfu->active && !p2p->active))
  {
    // we only have one connection so it can be switched immediately
    // or none of the links are active
    p2p->active = true;
    setConnection(p2p->outIndex, true);
  }
  else if (sfu->active)
  {
    // delayed switch to P2P link
    nextSwitch_ = EVALUATION_INTERVAL/2; // wait frames until we switch to P2P
    sfu->rtpSender->stopForwarding(sfu->ssrc, nextSwitch_);
    linksToSwitch_.push_back({p2p, sfu});
  }
  else
  {
    Logger::getLogger()->printWarning(this, "P2P connection is already active, no need to switch");
  }
}


void HybridFilter::delayedSwitchToSFU(std::shared_ptr<LinkInfo> p2p, std::shared_ptr<LinkInfo> sfu)
{
  if (cnameToLinks_.size() == 1 || (!sfu->active && !p2p->active))
  {
    p2p->active = false;
    setConnection(p2p->outIndex, false);
  }
  else if (!sfu->active)
  {
    nextSwitch_ = EVALUATION_INTERVAL/2; // wait frames until we switch to SFU
    sfu->rtpSender->startForwarding(sfu->ssrc, nextSwitch_);
    linksToSwitch_.push_back({p2p, sfu});
  }
  else
  {
    Logger::getLogger()->printWarning(this, "SFU connection is already active, no need to switch");
  }
}

void HybridFilter::fullBandwidthEvaluation()
{
  bool needSFU = false;
  int p2pLinks = 0;
  for (auto& pair : cnameToLinks_)
  {
    if (pair.second.p2p && !pair.second.sfu) // only P2P link
    {
      setConnection(pair.second.p2p->outIndex, true);
      ++p2pLinks;
    }
    else if (!pair.second.p2p && pair.second.sfu) // only SFU link
    {
      setConnection(pair.second.sfu->outIndex, true);
      needSFU = true;
    }
    else if (pair.second.p2p && pair.second.sfu) // both available
    {
      // only switch if p2p link actually reduces latency
      if (pair.second.p2p->latestsRtt < pair.second.sfu->latestsRtt)
      {
        ++p2pLinks;
        if (!pair.second.p2p->active)
        {
          delayedSwitchToP2P(pair.second.p2p, pair.second.sfu);
        }
      }
      else
      {
        Logger::getLogger()->printWarning(this, "SFU connection is faster for some reason than P2P");
        if (!pair.second.sfu->active)
        {
          delayedSwitchToSFU(pair.second.p2p, pair.second.sfu);
        }

        needSFU = true;
      }
    }
  }

  QString sfuStatus = needSFU ? "enabled" : "disabled";
  if (sfuIndex_ > -1)
  {
    // disables the sfu connection if we don't need it
    setConnection(sfuIndex_, needSFU);
  }
  else
  {
    sfuStatus = "not available";
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Full-bandwidth evaluation",
                                  {"P2P Links", "SFU Status"},
                                  {QString::number(p2pLinks) + "/" + QString::number(cnameToLinks_.size()),
                                   sfuStatus});
}
