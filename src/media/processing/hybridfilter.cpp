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
const int SYNC_PERIOD_IN_FRAMES = 60; // the period after which delayed switch is applied


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
    Logger::getLogger()->printNormal(this, "Created new LinkInfo for cname",
                                    {"CNAME","OutIndex"}, {cname, QString::number(outIdx)});
  }

  if (type == LINK_P2P)
  {
    if (entry->p2pRTPSender == nullptr)
    {
      Logger::getLogger()->printNormal(this, "Adding P2P link",
                                      {"CNAME", "SSRC", "SenderPtr"},
                                      {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});

      entry->p2pActive = false; // we use SFU only at the start
      setOutputStatus(outIdx, entry->p2pActive);
    }
    else
    {
      // TODO: The pointer that arrives here when adding new participants is for newest participant, which breasks existing connections
      Logger::getLogger()->printUnimplemented(this, "We should update existing index in case participants have left, but the index is not correct");

      if (entry->p2pRTPSender != rtpSender)
      {
        Logger::getLogger()->printWarning(this, "P2P RTP sender pointer changed for existing link");
      }
      if (entry->p2pSSRC != ssrc)
      {
        Logger::getLogger()->printWarning(this, "P2P SSRC changed for existing link");
      }
      
      return;
    }

    entry->p2pSSRC = ssrc;
    entry->p2pRTPSender = rtpSender;
    entry->p2pOutIndex = outIdx;

    QObject::connect(rtpSender.get(), &UvgRTPSender::rttReceived,
                     this, &HybridFilter::recordRTT);
  }
  else if (type == LINK_SFU)
  {
    Logger::getLogger()->printNormal(this, "Adding SFU link",
                                    {"CNAME","SSRC","SenderPtr"},
                                    {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});
    entry->sfuSSRC = ssrc;

    // SFU sender is a shared sender
    if (sfuRTPSender_ == nullptr)
    {
      Logger::getLogger()->printNormal(this, "Setting RTP sender for SFU link (shared sender)");
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
    Logger::getLogger()->printNormal(this, "Known SSRCs at time of unknown RTT",
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
    //Logger::getLogger()->printNormal(this, "Processing input",
    //                                 {"Input size"},
    //                                 {QString::number(input->data_size)});

    ++count_;
    if (count_ % EVALUATION_INTERVAL == 0)
    {
      triggerReEvaluation_ = true;
      Logger::getLogger()->printNormal(this, "Evaluation triggered by frame count",
                                      {"count","interval"}, {QString::number(count_), QString::number(EVALUATION_INTERVAL)});
    }

    if (count_ % DUMMY_INTERVAL == 0)
    {
      sendDummies(); // to get latency measurements for inactive connections
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
          // Delayed switch of links, needs to be synchronized with sfu server
          for (const auto& link : linksToSwitch_)
          {
            if (!link)
              continue;

            if (link->p2pActive)
            {
              Logger::getLogger()->printNormal(this, "Switch from P2P to SFU for SSRC " +
                                              QString::number(link->p2pSSRC) + " to " + QString::number(link->sfuSSRC));

              link->p2pActive = false; // turn P2P off
              setConnection(link->p2pOutIndex, link->p2pActive);
            }
            else
            {
              Logger::getLogger()->printNormal(this, "Switch from SFU to P2P for SSRC " +
                                                         QString::number(link->sfuSSRC) + " to " + QString::number(link->p2pSSRC));

              link->p2pActive = true; // turn P2P on
              setConnection(link->p2pOutIndex, link->p2pActive);
              // Note: SFU is disabled by bandwidth evaluation
            }
          }
          linksToSwitch_.clear();

          // Apply SFU state decision from evaluation
          applySfuState(pendingSfuActive_);
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


void HybridFilter::applySfuState(bool needSFU)
{
  if (!sfuRTPSender_)
    return;

  sfuActive_ = needSFU;
  setConnection(sfuOutIndex_, sfuActive_);
  Logger::getLogger()->printNormal(this, "Applying SFU state",
                                  "SFU State", sfuActive_ ? "enabled" : "disabled");
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
  pendingSfuActive_ = true; // we need sfu if not all connections are P2P
}


void HybridFilter::delayedSwitchToP2P(std::shared_ptr<LinkInfo> linkInfo)
{
  if (!linkInfo)
    return;

  if (cnameToLinks_.size() == 1) // immediate switch
  {
    Logger::getLogger()->printNormal(this, "Switching to P2P connection immediately for single connection",
                                     "P2P SSRC", QString::number(linkInfo->p2pSSRC));

    linkInfo->p2pActive = true;
    setConnection(linkInfo->p2pOutIndex, true);

    applySfuState(false);
  }
  else if (!linkInfo->p2pActive) // delayed switch
  {
    Logger::getLogger()->printNormal(this, "Scheduling delayed switch to P2P connection",
                                    {"SFU SSRC","P2P SSRC"},
                                    {QString::number(linkInfo->sfuSSRC), QString::number(linkInfo->p2pSSRC)});

    nextSwitch_ = SYNC_PERIOD_IN_FRAMES; // wait frames until we switch to P2P
    if (sfuRTPSender_)
    {
      // sync with sfu server
      sfuRTPSender_->stopForwarding(linkInfo->sfuSSRC, nextSwitch_);
    }
    linksToSwitch_.push_back(linkInfo);
  }
  else
  {
    Logger::getLogger()->printWarning(this, "P2P connection is already active, no need to switch");
  }
}


void HybridFilter::delayedSwitchToSFU(std::shared_ptr<LinkInfo> linkInfo)
{
  if (!linkInfo)
    return;

  if (cnameToLinks_.size() == 1) // immediate switch
  {
    if (sfuRTPSender_)
    {
      Logger::getLogger()->printNormal(this, "Switching to SFU connection immediately for single connection",
                                       "SFU SSRC", QString::number(linkInfo->sfuSSRC));

      applySfuState(true);

      linkInfo->p2pActive = false;
      setConnection(linkInfo->p2pOutIndex, false);
    }
  }
  else if (!sfuActive_) // delayed switch
  {
    if (sfuRTPSender_)
    {
      Logger::getLogger()->printNormal(this, "Scheduling delayed switch to SFU connection",
                                      {"P2P SSRC","SFU SSRC"},
                                      {QString::number(linkInfo->p2pSSRC), QString::number(linkInfo->sfuSSRC)});
      nextSwitch_ = SYNC_PERIOD_IN_FRAMES; // wait frames until we switch to SFU
      // sync with sfu server
      sfuRTPSender_->startForwarding(linkInfo->sfuSSRC, nextSwitch_);
      linksToSwitch_.push_back(linkInfo);
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
      if (entry->latestsP2PRtt <= 0.0 && entry->latestsSFURtt <= 0.0)
      {
        // no valid measurements, keep existing state
        Logger::getLogger()->printWarning(this, "No valid RTT measurements for links, keeping existing state",
                                         {"CNAME", "P2P RTT", "SFU RTT"}, {pair.first,
                                          QString::number(entry->latestsP2PRtt) + " ms",
                                          QString::number(entry->latestsSFURtt) + " ms"});
      }
      else if (entry->latestsP2PRtt < entry->latestsSFURtt)
      {
        ++p2pLinks;
        if (!entry->p2pActive)
        {
          Logger::getLogger()->printNormal(this, "Switch link to P2P",
                                          {"CNAME", "Expected latency reduction", "SSRC"},
                                          {pair.first, QString::number(entry->latestsSFURtt - entry->latestsP2PRtt) + " ms",
                                           QString::number(entry->p2pSSRC)});
          delayedSwitchToP2P(entry);
        }
      }
      else
      {
        if (entry->p2pActive)
        {
          Logger::getLogger()->printNormal(this, "Switch link to SFU",
                                           {"CNAME", "Expected latency increase", "SSRC"},
                                           {pair.first, QString::number(entry->latestsSFURtt - entry->latestsP2PRtt) + " ms",
                                            QString::number(entry->sfuSSRC)});
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

  Logger::getLogger()->printNormal(this, "Full-bandwidth evaluation",
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

  Logger::getLogger()->printNormal(this, "Partial-bandwidth evaluation",
                                  {"Connection bandwidth", "maxP2PConnections", "P2P Links"},
                                  {QString::number(connectionBandwidth), QString::number(maxP2PConnections),
                                   QString::number(p2pLinks) + "/" + QString::number(cnameToLinks_.size())});
}
