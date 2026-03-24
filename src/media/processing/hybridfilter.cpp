#include "hybridfilter.h"

#include "hybridslavefilter.h"
#include "media/delivery/uvgrtpsender.h"
#include "media/resourceallocator.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include "logger.h"

#include "settingskeys.h"
#include "common.h"
#include "statisticsinterface.h"

#include <QSize>

const uint8_t MAX_RTT_MEASUREMENTS = 64; // Maximum number of RTT samples to keep
const int EVALUATION_INTERVAL = 320; // Number of frames between evaluations
const int DUMMY_INTERVAL = 160; // Number of frames between dummy packets
const int SYNC_PERIOD_IN_FRAMES = 120; // the period after which delayed switch is applied
const int P2P_RTT_THRESHOLD_MS = 10; // Minimum RTT improvement to consider P2P switch
const int DUMMY_SESSION_BANDWIDTH_KBPS = 64; // low but non-zero; used to reduce RTCP traffic on inactive links


HybridFilter::HybridFilter(QString id, StatisticsInterface *stats,
      std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Hybrid", stats, hwResources, DT_HEVCVIDEO, DT_HEVCVIDEO),
    triggerReEvaluation_(false),
    sfuActive_(false),
    sfuOutIndex_(-1),
    sfuRTPSender_(nullptr),
    count_(0),
    framerateNumerator_(0),
    framerateDenominator_(0),
    nextSwitchTimestamp_(0)
{
  updateSettings();

  // Randomize initial count to stagger evaluation timing across clients
  uint16_t intraPeriod = (uint16_t)settingValue(SettingsKey::videoIntra);
  uint16_t randomMultiple = QRandomGenerator::global()->bounded(5); // 0-4
  count_ = randomMultiple * intraPeriod;

  Logger::getLogger()->printNormal(this, "Randomized initial count for staggered evaluations",
                                   {"IntraPeriod", "Multiplier", "InitialCount"},
                                   {QString::number(intraPeriod), QString::number(randomMultiple),
                                    QString::number(count_)});
}


HybridFilter::~HybridFilter()
{
  cnameToLinks_.clear();
}


void HybridFilter::updateSettings()
{
  Filter::updateSettings();

  sipTimestampInterval_ = settingValue(SettingsKey::sipTimestampInterval);

  if (settingValue(SettingsKey::videoFileEnabled))
  {
    framerateNumerator_ = settingValue(SettingsKey::videoFileFramerate);
    framerateDenominator_ = 1;
  }
  else
  {
    framerateNumerator_ = settingValue(SettingsKey::videoFramerateNumerator);
    framerateDenominator_ = settingValue(SettingsKey::videoFramerateDenominator);
  }

  Logger::getLogger()->printNormal(this, "Updated framerate for timestamp calculation",
                                  {"Numerator", "Denominator"},
                                  {QString::number(framerateNumerator_), QString::number(framerateDenominator_)});
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
                                    {"CNAME", "OutIndex"}, {cname, QString::number(outIdx)});
  }

  if (type == LINK_P2P)
  {
    addP2PLink(entry, outIdx, ssrc, cname, rtpSender);
  }
  else if (type == LINK_SFU)
  {
    addSFULink(entry, outIdx, ssrc, cname, rtpSender);
  }
}

void HybridFilter::addP2PLink(std::shared_ptr<LinkInfo>& entry,
                              int outIdx,
                              uint32_t ssrc,
                              const QString& cname,
                              std::shared_ptr<UvgRTPSender> rtpSender)
{
  if (entry->p2pRTPSender == nullptr)
  {
    Logger::getLogger()->printNormal(this, "Adding P2P link",
                                     {"CNAME", "SSRC", "SenderPtr"},
                                     {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});

    // Enable P2P by default only if there is no SFU present.
    entry->p2pActive = (sfuRTPSender_ == nullptr);

    setConnection(outIdx, entry->p2pActive, rtpSender);
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
  // Track that we now have a P2P sender registered (used by evaluator)
  ++p2pLinkCount_;

  QObject::connect(rtpSender.get(), &UvgRTPSender::rttReceived,
                   this, &HybridFilter::recordRTT,
                   Qt::UniqueConnection);
}


void HybridFilter::addSFULink(std::shared_ptr<LinkInfo>& entry,
                              int outIdx,
                              uint32_t ssrc,
                              const QString& cname,
                              std::shared_ptr<UvgRTPSender> rtpSender)
{
  Logger::getLogger()->printNormal(this, "Adding SFU link",
                                   {"CNAME","SSRC","SenderPtr"},
                                   {cname, QString::number(ssrc), QString::number((qulonglong)rtpSender.get())});
  entry->sfuSSRC = ssrc;


  if (!entry->p2pRTPSender)
  {
    entry->p2pActive = false;
  }

  // Always ensure RTT signal is connected for the SFU sender instance.
  // (UniqueConnection prevents duplicates.)
  if (rtpSender)
  {
    QObject::connect(rtpSender.get(), &UvgRTPSender::rttReceived,
                     this, &HybridFilter::recordRTT,
                     Qt::UniqueConnection);
  }

  // SFU sender is a shared sender
  if (sfuRTPSender_ == nullptr)
  {
    Logger::getLogger()->printNormal(this, "Setting RTP sender for SFU link (shared sender)");
    sfuRTPSender_ = rtpSender;
    sfuActive_ = true; // SFU link is active at the start
    sfuOutIndex_ = outIdx;
    setConnection(outIdx, sfuActive_, sfuRTPSender_);

    // Disable any already-active P2P links now that SFU is available, evaluation may enable them later
    for (auto& kv : cnameToLinks_)
    {
      const std::shared_ptr<LinkInfo>& link = kv.second;

      if (link && link->p2pActive && link->p2pOutIndex >= 0)
      {
        link->p2pActive = false;
        setConnection(link->p2pOutIndex, link->p2pActive, link->p2pRTPSender);
      }
    }

  }
}


void HybridFilter::setLowRtcpMode(const std::shared_ptr<UvgRTPSender>& sender, bool enabled)
{
  if (!sender)
    return;

  if (enabled)
    sender->setTemporarySessionBandwidthKbps(DUMMY_SESSION_BANDWIDTH_KBPS);
  else
    sender->clearTemporarySessionBandwidth();
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
      if (entry->p2pRTT.empty())
      {
        Logger::getLogger()->printNormal(this, "First RTT sample received",
                                         {"CNAME","Type","RTT"}, {pair.first, "P2P", QString::number(rtt)});
      }

      entry->p2pRTT.push_back(rtt);
      if (entry->p2pRTT.size() > MAX_RTT_MEASUREMENTS)
      {
        entry->p2pRTT.pop_front();
      }

      // Keep cached RTT up-to-date even when we are not running hybrid re-evaluations
      // (pure P2P mesh case). Use average to smooth jitter.
      entry->latestsP2PRtt = averageRTT(entry->p2pRTT);

      foundSSRC = true;
      break;
    }

    if (entry->sfuSSRC == ssrc)
    {
      if (entry->sfuRTT.empty())
      {
        Logger::getLogger()->printNormal(this, "First RTT sample received",
                                         {"CNAME","Type","RTT"}, {pair.first, "SFU", QString::number(rtt)});
      }

      entry->sfuRTT.push_back(rtt);
      if (entry->sfuRTT.size() > MAX_RTT_MEASUREMENTS)
      {
        entry->sfuRTT.pop_front();
      }

      // Keep cached RTT up-to-date even when we are not running hybrid re-evaluations
      // (pure SFU case).
      entry->latestsSFURtt = averageRTT(entry->sfuRTT);
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
    }

    if (count_ % DUMMY_INTERVAL == 0)
    {
      if (p2pLinkCount_ > 0 && sfuRTPSender_ != nullptr)
      {
        sendDummies(input->rtpTimestamp); // to get latency measurements for inactive connections
      }
      else
      {
        Logger::getLogger()->printNormal(this, "Skipping dummy packet: not hybrid (no P2P or no SFU)");
      }
    }

    if (triggerReEvaluation_)
    {
      if (p2pLinkCount_ == 0 || sfuRTPSender_ == nullptr)
      {
        Logger::getLogger()->printNormal(this, "Skipping re-evaluation: not hybrid (no P2P or no SFU)");
      }
      else if (input->rtpTimestamp != 0)
      {
        reEvaluateConnections(input->rtpTimestamp);
      }

      triggerReEvaluation_ = false;
    }

    // Check and execute switches BEFORE sending output to ensure connection state
    // matches the SFU's forwarding state at the exact same timestamp
    if (nextSwitchTimestamp_ != 0 && input->rtpTimestamp != 0)
    {
      // Use signed arithmetic to handle RTP timestamp rollover
      int32_t delta = static_cast<int32_t>(input->rtpTimestamp - nextSwitchTimestamp_);
      if (delta >= 0)
      {
        Logger::getLogger()->printNormal(this, "Executing switches at RTP timestamp",
                                        {"Current", "Scheduled", "Delta"},
                                        {QString::number(input->rtpTimestamp), 
                                         QString::number(nextSwitchTimestamp_),
                                         QString::number(delta)});
        executeSwitches();
        nextSwitchTimestamp_ = 0;
      }
    }

    // Report encoded frame statistics: size, aggregate bandwidth (size * active connections),
    // encoding time (ms), resolution and PSNRs (if available).
    if (input)
    {
      uint32_t frameSize = input->data_size;

      // Some timestamp interval modes add extra bytes that should not be counted as encoded frame payload.
      // If sipTimestampInterval == 1, subtract the known overhead (33 bytes) from the reported frame size.
      if (sipTimestampInterval_ == 1)
      {
        constexpr uint32_t kTimestampIntervalOverheadBytes = 33;
        frameSize = (frameSize >= kTimestampIntervalOverheadBytes)
                      ? (frameSize - kTimestampIntervalOverheadBytes)
                      : 0;
      }

      // Count active outgoing connections: active P2P links + SFU (if enabled)
      int activeConnections = 0;
      for (const auto& kv : cnameToLinks_)
      {
        const std::shared_ptr<LinkInfo>& e = kv.second;
        if (!e) continue;
        if (e->p2pRTPSender && e->p2pActive && e->p2pOutIndex >= 0)
          ++activeConnections;
      }
      if (sfuRTPSender_ && sfuActive_)
        ++activeConnections;

      uint64_t bw64 = static_cast<uint64_t>(frameSize) * static_cast<uint64_t>(activeConnections);
      // Account for extra bandwidth overhead (RTP/RTCP headers etc.) of 10  %
      double adjusted = static_cast<double>(bw64) / 0.9;
      uint64_t bwAdj64 = adjusted > static_cast<double>(UINT64_MAX) ? UINT64_MAX : static_cast<uint64_t>(adjusted);
      uint32_t bandwidth = bwAdj64 > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(bwAdj64);

      int64_t now = clockNowMs();
      int64_t encTime64 = (input->creationTimestamp >= 0) ? (now - input->creationTimestamp) : 0;
      uint32_t encodingTime = encTime64 > 0 ? static_cast<uint32_t>(encTime64) : 0;

      QSize resolution(0,0);
      float psnrY = -1.0f, psnrU = -1.0f, psnrV = -1.0f;
      if (input->vInfo)
      {
        resolution.setWidth(input->vInfo->width);
        resolution.setHeight(input->vInfo->height);
        psnrY = input->vInfo->psnrY;
        psnrU = input->vInfo->psnrU;
        psnrV = input->vInfo->psnrV;
      }

      // Compute network latency from the latest RTT sample per link (divide by two to approximate
      // one-way network latency). Using the latest sample is preferable for per-frame reporting
      // over a smoothed/moving average.
      double sumRtt = 0.0;
      int rttCount = 0;
      for (const auto& kv : cnameToLinks_)
      {
        const std::shared_ptr<LinkInfo>& e = kv.second;
        if (!e) continue;

        // P2P active for this participant
        const bool hasUsableP2P = (e->p2pRTPSender && e->p2pActive);
        const bool shouldUseSFU = (e->sfuSSRC != 0 && sfuActive_ && (!e->p2pActive || !e->p2pRTPSender));

        if (hasUsableP2P)
        {
          if (!e->p2pRTT.empty() && e->p2pRTT.back() > 0.0)
          {
            sumRtt += (e->p2pRTT.back() / 2.0);
            ++rttCount;
          }
        }
        else if (shouldUseSFU)
        {
          // Participant is served via SFU and SFU is active
          if (!e->sfuRTT.empty() && e->sfuRTT.back() > 0.0)
          {
            sumRtt += (e->sfuRTT.back() / 2.0);
            ++rttCount;
          }
        }
      }

      int64_t networkLatencyMs = -1;
      if (rttCount > 0)
      {
        // Round to nearest integer millisecond
        networkLatencyMs = static_cast<int64_t>(sumRtt / static_cast<double>(rttCount) + 0.5);
      }

      getStats()->encodedVideoFrame(frameSize, bandwidth, encodingTime, resolution,
                                    psnrY, psnrU, psnrV, networkLatencyMs, input->creationTimestamp);
    }

    sendOutput(std::move(input));
    input = getInput();
  }
}


void HybridFilter::executeSwitches()
{
  // Delayed switch of links, needs to be synchronized with sfu server
  for (const auto& link : linksToSwitch_)
  {
    if (!link)
      continue;

    // Use the explicitly scheduled action instead of toggling based on current state.
    if (link->pendingSwitch == LinkInfo::PendingToP2P)
    {
      Logger::getLogger()->printNormal(this, "Switch from SFU to P2P for SSRC " +
                                         QString::number(link->sfuSSRC) + " to " + QString::number(link->p2pSSRC));

      link->p2pActive = true;
      if (link->p2pOutIndex >= 0)
        setConnection(link->p2pOutIndex, true, link->p2pRTPSender);
    }
    else if (link->pendingSwitch == LinkInfo::PendingToSFU)
    {
      Logger::getLogger()->printNormal(this, "Switch from P2P to SFU for SSRC " +
                                         QString::number(link->p2pSSRC) + " to " + QString::number(link->sfuSSRC));

      link->p2pActive = false;
      if (link->p2pOutIndex >= 0)
        setConnection(link->p2pOutIndex, false, link->p2pRTPSender);
    }

    // Clear pending flag after execution
    link->pendingSwitch = LinkInfo::PendingNone;
  }

  linksToSwitch_.clear();

  // Apply SFU state decision from evaluation
  applySfuState(pendingSfuActive_);
}


void HybridFilter::sendDummies(uint32_t currentTimestamp)
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
  newDummy->creationTimestamp = clockNowMs();
  newDummy->presentationTimestamp = newDummy->creationTimestamp;
  newDummy->rtpTimestamp = currentTimestamp;

  newDummy->type = output_;
  newDummy->data_size = sizeof(dummy_packet);
  newDummy->data = std::unique_ptr<uchar[]>(new uchar[sizeof(dummy_packet)]);

  if (newDummy->vInfo)
  {
    newDummy->vInfo->keyframe = true;
  }

  memcpy(newDummy->data.get(), dummy_packet, sizeof(dummy_packet));
  sendOutput(std::move(newDummy), true); // inverse = true to send data to inactive paths
}


void HybridFilter::setConnection(int index, bool status, const std::shared_ptr<UvgRTPSender>& sender)
{
  setOutputStatus(index, status);

  // Reduce RTCP traffic on inactive (dummy-only) connections by temporarily
  // lowering uvgRTP session bandwidth. Keep it non-zero for RTT probing.
  if (sender)
    setLowRtcpMode(sender, !status);

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
  setConnection(sfuOutIndex_, sfuActive_, sfuRTPSender_);
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


void HybridFilter::reEvaluateConnections(uint32_t currentTimestamp)
{
  const int64_t availableBandwidth = static_cast<int64_t>(settingValue(SettingsKey::sipUpBandwidth));

  // Per-uplink payload bitrate (video + audio). We convert this to an estimated
  // on-wire bitrate below when computing how many parallel uplinks fit.
  int64_t payloadBandwidth = static_cast<int64_t>(getHWManager()->getEncoderBitrate(output_));

  for (auto& slave : slaves_)
  {
    // add audio bitrate
    payloadBandwidth += static_cast<int64_t>(slave->getBitrate());
  }

  if (payloadBandwidth <= 0 || availableBandwidth <= 0)
  {
    Logger::getLogger()->printError(this, "Connection bandwidth is zero or negative, cannot re-evaluate connections");
    return;
  }

  // Convert payload bitrate to an estimated total (payload + RTP/RTCP overhead).
  // Our overhead constant represents a fraction of total bandwidth, so:
  //   payload = total * (1 - overhead)  =>  total = payload / (1 - overhead)
  const double denom = (1.0 - static_cast<double>(TRANSMISSION_OVERHEAD));
  if (denom <= 0.0)
  {
    Logger::getLogger()->printProgramError(this, "Invalid TRANSMISSION_OVERHEAD (>= 1.0), cannot re-evaluate");
    return;
  }
  const int64_t linkBandwidth = static_cast<int64_t>(std::ceil(static_cast<double>(payloadBandwidth) / denom));

  if (linkBandwidth <= 0)
  {
    Logger::getLogger()->printError(this, "Computed link bandwidth is zero or negative, cannot re-evaluate connections");
    return;
  }

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

  const int64_t maxTotalUplinks = availableBandwidth / linkBandwidth;
  int maxP2PConnections = static_cast<int>(std::max<int64_t>(0, maxTotalUplinks));

  // we can do all connections however we like
  if (static_cast<size_t>(maxP2PConnections) >= cnameToLinks_.size())
  {
    Logger::getLogger()->printNormal(this, "Bandwidth allows full evaluation",
                                    {"Available", "PerUplink", "MaxUplinks", "Participants"},
                                    {QString::number(static_cast<long long>(availableBandwidth)),
                                     QString::number(static_cast<long long>(linkBandwidth)),
                                     QString::number(static_cast<long long>(maxTotalUplinks)),
                                     QString::number(static_cast<unsigned long long>(cnameToLinks_.size()))});
    return fullBandwidthEvaluation(currentTimestamp);
  }

  // Reserve one uplink for SFU when we cannot afford P2P to everyone.
  // This is the intended place where results can look "one short" (P2P slots
  // become maxTotalUplinks - 1) when SFU must be active.
  maxP2PConnections = std::max(0, maxP2PConnections - 1);

  Logger::getLogger()->printNormal(this, "Bandwidth-limited evaluation",
                                  {"Available", "PerUplink", "MaxUplinks", "P2PSlots", "Participants"},
                                  {QString::number(static_cast<long long>(availableBandwidth)),
                                   QString::number(static_cast<long long>(linkBandwidth)),
                                   QString::number(static_cast<long long>(maxTotalUplinks)),
                                   QString::number(maxP2PConnections),
                                   QString::number(static_cast<unsigned long long>(cnameToLinks_.size()))});

  // Otherwise, use P2P where RTT benefit is biggest
  rankedBandwidthEvaluation(maxP2PConnections, static_cast<int>(std::min<int64_t>(linkBandwidth, INT_MAX)), currentTimestamp);
  pendingSfuActive_ = true; // we need sfu if not all connections are P2P
}


void HybridFilter::delayedSwitchToP2P(std::shared_ptr<LinkInfo> linkInfo, uint32_t currentTimestamp)
{
  if (!linkInfo)
    return;

  if (cnameToLinks_.size() == 1) // immediate switch
  {
    Logger::getLogger()->printNormal(this, "Switching to P2P connection immediately for single connection",
                                     "P2P SSRC", QString::number(linkInfo->p2pSSRC));

    linkInfo->p2pActive = true;
    setConnection(linkInfo->p2pOutIndex, true, linkInfo->p2pRTPSender);

    applySfuState(false);
  }
  else if (!linkInfo->p2pActive) // delayed switch
  {
    if (sfuRTPSender_)
    {
      // Calculate future timestamp for when the switch will occur
      uint32_t futureTimestamp = updateVideoRtpTimestamp(currentTimestamp, framerateNumerator_, framerateDenominator_, SYNC_PERIOD_IN_FRAMES);
      nextSwitchTimestamp_ = futureTimestamp;

      Logger::getLogger()->printNormal(this, "Scheduling delayed switch to P2P connection",
                                      {"SFU SSRC","P2P SSRC", "Current TS", "Future TS"},
                                      {QString::number(linkInfo->sfuSSRC), QString::number(linkInfo->p2pSSRC),
                                       QString::number(currentTimestamp), QString::number(futureTimestamp)});

      // sync with sfu server
      sfuRTPSender_->stopForwarding(linkInfo->sfuSSRC, futureTimestamp);
    }
    // Record the explicit pending switch and avoid duplicate scheduling for the same link
    linkInfo->pendingSwitch = LinkInfo::PendingToP2P;
    if (std::find(linksToSwitch_.begin(), linksToSwitch_.end(), linkInfo) == linksToSwitch_.end())
    {
      linksToSwitch_.push_back(linkInfo);
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Link is already scheduled for switch to P2P, skipping duplicate");
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "P2P connection is already active, no need to switch");
  }
}


void HybridFilter::delayedSwitchToSFU(std::shared_ptr<LinkInfo> linkInfo, uint32_t currentTimestamp)
{
  if (!linkInfo)
    return;

  if (!sfuRTPSender_ || linkInfo->sfuSSRC == 0)
  {
    Logger::getLogger()->printWarning(this, "Cannot switch to SFU: SFU RTP sender not available");
    return;
  }

  if (cnameToLinks_.size() == 1) // immediate switch
  {
    Logger::getLogger()->printNormal(this, "Switching to SFU connection immediately for single connection",
                                     "SFU SSRC", QString::number(linkInfo->sfuSSRC));

    applySfuState(true);

    linkInfo->p2pActive = false;
    setConnection(linkInfo->p2pOutIndex, false, linkInfo->p2pRTPSender);
  }
  else if (linkInfo->p2pActive) // delayed switch
  {
    // Calculate future timestamp for when the switch will occur
    uint32_t futureTimestamp = updateVideoRtpTimestamp(currentTimestamp, framerateNumerator_, framerateDenominator_, SYNC_PERIOD_IN_FRAMES);
    nextSwitchTimestamp_ = futureTimestamp;

    Logger::getLogger()->printNormal(this, "Scheduling delayed switch to SFU connection",
                                    {"P2P SSRC","SFU SSRC", "Current TS", "Future TS", "SFU Active"},
                                    {QString::number(linkInfo->p2pSSRC), QString::number(linkInfo->sfuSSRC),
                                     QString::number(currentTimestamp), QString::number(futureTimestamp),
                                     sfuActive_ ? "true" : "false"});

    // Ensure SFU stays enabled while we are switching participants back to it.
    pendingSfuActive_ = true;

    // sync with sfu server
    sfuRTPSender_->startForwarding(linkInfo->sfuSSRC, futureTimestamp);

    // Record the explicit pending switch and avoid duplicate scheduling for the same link
    linkInfo->pendingSwitch = LinkInfo::PendingToSFU;
    if (std::find(linksToSwitch_.begin(), linksToSwitch_.end(), linkInfo) == linksToSwitch_.end())
    {
      linksToSwitch_.push_back(linkInfo);
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Link is already scheduled for switch to SFU, skipping duplicate");
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Link is already on SFU (P2P not active), no need to switch");
  }
}


void HybridFilter::fullBandwidthEvaluation(uint32_t currentTimestamp)
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
      setConnection(entry->p2pOutIndex, true, entry->p2pRTPSender);
      ++p2pLinks;
    }
    else if (!hasP2P && hasSFU) // only SFU link
    {
      setConnection(sfuOutIndex_, true, sfuRTPSender_);
      needSFU = true;
    }
    else if (hasP2P && hasSFU) // both available
    {
      // only switch if p2p link actually reduces latency
      if (entry->latestsP2PRtt <= 0.0 || entry->latestsSFURtt <= 0.0)
      {
        // no valid measurements, keep existing state
        Logger::getLogger()->printWarning(this, "No valid RTT measurements for links, keeping existing state",
                                         {"CNAME", "P2P RTT", "SFU RTT"}, {pair.first,
                                          QString::number(entry->latestsP2PRtt) + " ms",
                                          QString::number(entry->latestsSFURtt) + " ms"});

        if (!entry->p2pActive)
        {
          needSFU = true;
        }
      }
      else if (entry->latestsP2PRtt + P2P_RTT_THRESHOLD_MS < entry->latestsSFURtt)
      {
        ++p2pLinks;
        if (!entry->p2pActive)
        {
          Logger::getLogger()->printNormal(this, "Switch link to P2P",
                                          {"CNAME", "Expected latency reduction", "SSRC"},
                                          {pair.first, QString::number(entry->latestsSFURtt - entry->latestsP2PRtt) + " ms",
                                           QString::number(entry->p2pSSRC)});
          delayedSwitchToP2P(entry, currentTimestamp);
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
          delayedSwitchToSFU(entry, currentTimestamp);
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


void HybridFilter::rankedBandwidthEvaluation(const int maxP2PConnections, int connectionBandwidth, uint32_t currentTimestamp)
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
    bool shouldBeP2P = (i < maxP2PConnections) && (rankedConnections[i].rttBenefit > P2P_RTT_THRESHOLD_MS);

    if (shouldBeP2P)
    {
      if (!rankedConnections[i].p2plink->p2pActive)
      {
        delayedSwitchToP2P(rankedConnections[i].p2plink, currentTimestamp);
      }
      ++p2pLinks;
    }
    else
    {
      if (rankedConnections[i].p2plink->p2pActive)
      {
        delayedSwitchToSFU(rankedConnections[i].p2plink, currentTimestamp);
      }
    }
  }

  Logger::getLogger()->printNormal(this, "Partial-bandwidth evaluation",
                                  {"Connection bandwidth", "maxP2PConnections", "P2P Links"},
                                  {QString::number(connectionBandwidth), QString::number(maxP2PConnections),
                                   QString::number(p2pLinks) + "/" + QString::number(cnameToLinks_.size())});
}
