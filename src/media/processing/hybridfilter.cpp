#include "hybridfilter.h"

#include "hybridslavefilter.h"
#include "media/delivery/uvgrtpsender.h"
#include "media/resourceallocator.h"

#include "logger.h"

#include "settingskeys.h"
#include "common.h"

const uint8_t MAX_RTT_MEASUREMENTS = 64; // Maximum number of RTT samples to keep
const int EVALUATION_INTERVAL = 640; // Number of frames between evaluations
const int DUMMY_INTERVAL = 160; // Number of frames between dummy packets


HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO),
    triggerReEvaluation_(false),
    sfuActive_(false),
    sfuOutIndex_(-1),
    sfuRTPSender_(nullptr),
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
  if (sizeOfOutputConnections() == 0)
  {
    Logger::getLogger()->printError("Hybrid", "No output connections available for link");
    return;
  }

    int outIdx = -1;
  if (rtpSender)
  {
    // UvgRTPSender inherits from Filter, so we can lookup its index
    outIdx = getOutConnectionIndex(std::static_pointer_cast<Filter>(rtpSender));
  }

  if (outIdx < 0)
  {
    Logger::getLogger()->printError("Hybrid", "RTP sender not found among output connections");
    return;
  }

  // Ensure we have a LinkInfo entry for this cname
  std::shared_ptr<LinkInfo>& entry = cnameToLinks_[cname];
  if (!entry)
  {
    entry = std::make_shared<LinkInfo>();
    entry->p2pActive = false;
    entry->p2pOutIndex = -1;
    entry->p2pRTPSender = nullptr;
    entry->sfuSSRC = 0;
    entry->latestsP2PRtt = 0.0;
    entry->latestsSFURtt = 0.0;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Created new LinkInfo for cname",
                                    {"CNAME","OutIndex"}, {cname, QString::number(outIdx)});
  }

  if (type == LINK_P2P)
  {
    if (entry->p2pRTPSender)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Overwriting existing P2P link",
                                      {"CNAME", "SSRC", "SenderPtr"},
                                      {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Adding P2P link",
                                      {"CNAME", "SSRC", "SenderPtr"},
                                      {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});
    }

    entry->p2pSSRC = ssrc;
    entry->p2pRTPSender = rtpSender;
    entry->p2pOutIndex = outIdx;
    entry->p2pActive = false; // we use SFU only at the start
    setOutputStatus(outIdx, entry->p2pActive);

    QObject::connect(rtpSender.get(), &UvgRTPSender::rttReceived,
                     this, &HybridFilter::recordRTT);
  }
  else if (type == LINK_SFU)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Adding SFU link",
                                    {"CNAME","SSRC","SenderPtr"},
                                    {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});
    entry->sfuSSRC = ssrc;

    // SFU sender is a shared sender
    if (sfuRTPSender_ == nullptr)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Setting RTP sender for SFU link (shared sender)");
      sfuRTPSender_ = rtpSender;
      sfuActive_ = true; // SFU link is active at the start
      sfuOutIndex_ = outIdx;
      setOutputStatus(outIdx, sfuActive_);

      QObject::connect(rtpSender.get(), &UvgRTPSender::rttReceived,
                       this, &HybridFilter::recordRTT);
    }
  }
}


void HybridFilter::recordRTT(uint32_t ssrc, double rtt)
{
  bool foundSSRC = false;

  for (auto& pair : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& entry = pair.second;
    if (!entry)
      continue;

    if (entry->p2pSSRC == ssrc)
    {
      entry->p2pRTT.push_back(rtt);
      if (entry->p2pRTT.size() > MAX_RTT_MEASUREMENTS)
      {
        entry->p2pRTT.pop_front();
      }
      foundSSRC = true;
      break;
    }

    if (entry->sfuSSRC == ssrc)
    {
      entry->sfuRTT.push_back(rtt);
      if (entry->sfuRTT.size() > MAX_RTT_MEASUREMENTS)
      {
        entry->sfuRTT.pop_front();
      }
      foundSSRC = true;
      break;
    }
  }

  if (!foundSSRC)
  {
    // Dump current known entries for diagnostics
    QStringList names;
    QStringList values;
    for (const auto& kv : cnameToLinks_)
    {
      const QString& key = kv.first;
      const std::shared_ptr<LinkInfo>& e = kv.second;
      if (!e) continue;
      names << "CNAME" << "p2pSSRC" << "sfuSSRC";
      values << key << QString::number(e->p2pSSRC) << QString::number(e->sfuSSRC);
    }
    Logger::getLogger()->printError("Hybrid", "RTT record for unknown SSRC: " + QString::number(ssrc));
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Known SSRCs at time of unknown RTT",
                                    names, values);
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
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Evaluation triggered by frame count",
                                      {"count","interval"}, {QString::number(count_), QString::number(EVALUATION_INTERVAL)});
    }

    if (count_ % DUMMY_INTERVAL == 0)
    {
      sendDummies(); // send dummies to get latency measurements for inactive connections
    }

    if (triggerReEvaluation_)
    {
      reEvaluateConnections();
      triggerReEvaluation_ = false;
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

          // Delayed switch of links, needs to be synchronized with sfu server
          for (const auto& link : linksToSwitch_)
          {
            if (!link)
              continue;

            bool p2pActive = link->p2pActive;

            if (link->p2pActive)
            {
              // switch from P2P -> SFU
              link->p2pActive = false;
              setConnection(link->p2pOutIndex, false);
              setConnection(sfuOutIndex_, true);
            }
            else
            {
              // switch from SFU -> P2P
              link->p2pActive = true;
              setConnection(link->p2pOutIndex, true);
              // Note: SFU is disabled by bandwidth evaluation
            }
          }
          linksToSwitch_.clear();
          // Apply SFU state decisions that were computed during evaluation.
          // This ensures SFU is not disabled immediately during evaluation
          // while a delayed switch to P2P is still pending.
          applySfuState(pendingSfuActive_, true);
        }
    }
  }
}


void HybridFilter::sendDummies()
{
  // the point of dummy is to send a harmless packet via all inactive media paths so they send RTCP SR
  uint8_t dummy_packet[] = {
      0x00, 0x00, 0x00, 0x01,
      0x4E,
      0x7B, 0x01,
      0x00,
      0x80
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


void HybridFilter::applySfuState(bool needSFU, bool immediate)
{
  if (!sfuRTPSender_)
    return;

  if (immediate)
  {
    sfuActive_ = needSFU;
    setConnection(sfuOutIndex_, sfuActive_);
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Applying SFU state",
                                    {"SFU State"}, {sfuActive_ ? "enabled" : "disabled"});
  }
  else
  {
    pendingSfuActive_ = needSFU;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "SFU state pending",
                                    {"SFU State"}, {pendingSfuActive_ ? "enabled" : "disabled"});
  }
}


double HybridFilter::averageRTT(const std::deque<double>& samples) const
{
  if (samples.empty())
    return 0.0;

  double sum = 0.0;
  for (const auto& s : samples)
  {
    sum += s;
  }

  return sum / static_cast<double>(samples.size());
}


void HybridFilter::reEvaluateConnections()
{
  const int networkBandwidth = settingValue(SettingsKey::sipUpBandwidth);
  int linkBandwidth = getHWManager()->getEncoderBitrate(output_);

  for (auto& slave : slaves_)
  {
    linkBandwidth += slave->getBitrate();;
  }

  linkBandwidth = linkBandwidth/0.95; // reserve 5% for overhead

  if (linkBandwidth <= 0)
  {
    Logger::getLogger()->printError(this, "Connection bandwidth is zero or negative, cannot re-evaluate connections");
    return;
  }

  const int maxP2PConnections = networkBandwidth / linkBandwidth;

  // calculate the average RTT for each cname/link entry
  for (auto& pair : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& entry = pair.second;
    if (!entry)
      continue;
    // update separate RTT averages for P2P and SFU
    double sum = 0.0;
    entry->latestsP2PRtt = averageRTT(entry->p2pRTT);
    entry->latestsSFURtt = averageRTT(entry->sfuRTT);
  }

  // we can do all connections however we like
  if (maxP2PConnections >= cnameToLinks_.size())
  {
    return fullBandwidthEvaluation();
  }

  // Otherwise, use P2P where RTT benefit is biggest
  rankedBandwidthEvaluation(maxP2PConnections, linkBandwidth);
}


void HybridFilter::delayedSwitchToP2P(std::shared_ptr<LinkInfo> p2p)
{
  if (!p2p)
    return;

  if (cnameToLinks_.size() == 1)
  {
    // we only have one connection so it can be switched immediately
    p2p->p2pActive = true;
    setConnection(p2p->p2pOutIndex, true);
    if (sfuRTPSender_)
    {
      sfuActive_ = false;
      setConnection(sfuOutIndex_, false);
    }
  }
  else if (!p2p->p2pActive)
  {
    // delayed switch to P2P link
    nextSwitch_ = EVALUATION_INTERVAL/2; // wait frames until we switch to P2P
    if (sfuRTPSender_)
      sfuRTPSender_->stopForwarding(p2p->sfuSSRC, nextSwitch_);
    linksToSwitch_.push_back(p2p);
  }
  else
  {
    Logger::getLogger()->printWarning(this, "P2P connection is already active, no need to switch");
  }
}


void HybridFilter::delayedSwitchToSFU(std::shared_ptr<LinkInfo> p2p)
{
  if (!p2p)
    return;

  if (cnameToLinks_.size() == 1)
  {
    // If this is the only connection we should enable SFU immediately
    if (sfuRTPSender_)
    {
      sfuActive_ = true;
      setConnection(sfuOutIndex_, true);

      p2p->p2pActive = false;
      setConnection(p2p->p2pOutIndex, false);
    }
  }
  else if (!sfuActive_)
  {
    if (sfuRTPSender_)
    {
      nextSwitch_ = EVALUATION_INTERVAL/2; // wait frames until we switch to SFU
      sfuRTPSender_->startForwarding(p2p->sfuSSRC, nextSwitch_);
      linksToSwitch_.push_back(p2p);
    }
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
    const std::shared_ptr<LinkInfo>& entry = pair.second;
    if (!entry)
      continue;

    bool hasP2P = static_cast<bool>(entry->p2pRTPSender);
    bool hasSFU = (entry->sfuSSRC != 0);

    if (hasP2P && !hasSFU) // only P2P link
    {
      setConnection(entry->p2pOutIndex, true);
      ++p2pLinks;
    }
    else if (!hasP2P && hasSFU) // only SFU link
    {
      setConnection(sfuOutIndex_, true);
      needSFU = true;
    }
    else if (hasP2P && hasSFU) // both available
    {
      // only switch if p2p link actually reduces latency
      if (entry->latestsP2PRtt > 0.0 && entry->latestsSFURtt > 0.0 && entry->latestsP2PRtt < entry->latestsSFURtt)
      {
        ++p2pLinks;
        if (!entry->p2pActive)
        {
          Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Switching to P2P connection",
                                          {"CNAME", "Expected latency reduction", "SSRC"},
                                          {pair.first, QString::number(entry->latestsSFURtt - entry->latestsP2PRtt) + " ms",
                                           QString::number(entry->p2pSSRC)});
          delayedSwitchToP2P(entry);
        }
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Switching to SFU connection",
                                        {"CNAME", "Expected latency reduction", "SSRC"},
                                        {pair.first, QString::number(entry->latestsP2PRtt - entry->latestsSFURtt) + " ms",
                                         QString::number(entry->sfuSSRC)});
        if (entry->p2pActive)
        {
          delayedSwitchToSFU(entry);
        }

        needSFU = true;
      }
    }
  }

  QString sfuStatus = needSFU ? "enabled" : "disabled";
  if (sfuRTPSender_)
  {
    // Do not toggle SFU immediately here; defer applying the change until
    // switches are executed so we avoid gaps caused by turning SFU off
    // while a delayed P2P switch is still in-flight.
    pendingSfuActive_ = needSFU;
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


void HybridFilter::rankedBandwidthEvaluation(const int maxP2PConnections, int connectionBandwidth)
{
  struct RankedConnection
  {
    std::shared_ptr<LinkInfo> p2plink;
    double rttBenefit; // higher is better
  };

  std::vector<RankedConnection> rankedConnections;

  for (auto& pair : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& entry = pair.second;
    if (!entry)
      continue;

    if (entry->p2pRTPSender && entry->sfuSSRC != 0)
    {
      RankedConnection ranked;
      ranked.p2plink = entry;
      // benefit is SFU RTT minus P2P RTT (higher means bigger win for P2P)
      ranked.rttBenefit = entry->latestsSFURtt - entry->latestsP2PRtt;
      rankedConnections.push_back(ranked);
    }
  }

  // Sort connections by RTT benefit (descending)
  std::sort(rankedConnections.begin(), rankedConnections.end(),
            [](const RankedConnection& a, const RankedConnection& b) {
              return a.rttBenefit > b.rttBenefit;
            });

  int p2pLinks = 0;
  // Enable P2P connections until we reach the limit
  for (int i = 0; i < rankedConnections.size(); ++i)
  {
    bool shouldBeP2P = (i < maxP2PConnections) && (rankedConnections[i].rttBenefit > 0);

    if (shouldBeP2P)
    {
      if (!rankedConnections[i].p2plink->p2pActive)
      {
        delayedSwitchToP2P(rankedConnections[i].p2plink);
      }
      ++p2pLinks;
    }
    else
    {
      if (rankedConnections[i].p2plink->p2pActive)
      {
        delayedSwitchToSFU(rankedConnections[i].p2plink);
      }
    }
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Partial-bandwidth evaluation",
                                  {"Connection bandwidth", "maxP2PConnections", "P2P Links"},
                                  {QString::number(connectionBandwidth), QString::number(maxP2PConnections),
                                   QString::number(p2pLinks) + "/" + QString::number(cnameToLinks_.size())});
}
