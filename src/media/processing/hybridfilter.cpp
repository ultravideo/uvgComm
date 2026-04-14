#include "hybridfilter.h"

#include "hybridslavefilter.h"
#include "media/delivery/uvgrtpsender.h"
#include "media/resourceallocator.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

#include "logger.h"

#include "settingskeys.h"
#include "common.h"
#include "statisticsinterface.h"

#include <QSize>

const uint8_t MAX_RTT_MEASUREMENTS = 16; // Maximum number of RTT samples to keep
const int EVALUATION_INTERVAL = 640; // Number of frames between evaluations
const int DUMMY_INTERVAL = 120; // needs to be sent at least once per RTCP period
const int RTT_SWITCH_THRESHOLD_MS = 10; // Bidirectional RTT deadband before switching paths
const int DUMMY_SESSION_BANDWIDTH_KBPS = 64; // low but non-zero; used to reduce RTCP traffic on inactive links
const int RTCP_WARMUP_SESSION_BANDWIDTH_KBPS = 256; // temporary boost until first SFU RTT sample
const double RTCP_AVG_PACKET_SIZE_OCTETS = 150.0;
const double SYNC_CUSHION_SECONDS = 0.25;
const double DEFAULT_SYNC_SESSION_BANDWIDTH_BPS = 100000.0;
const int MAX_SYNC_PERIOD_IN_FRAMES = 600; // Cap to avoid excessive switch latency.


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
}


HybridFilter::~HybridFilter()
{
  cnameToLinks_.clear();
}


bool HybridFilter::hasAnyP2PLinks() const
{
  for (const auto& kv : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& entry = kv.second;
    if (entry && entry->p2pRTPSender)
      return true;
  }

  return false;
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

    // If SFU is unavailable or currently disabled, activate the new P2P path immediately
    // so the link does not stay without any active forwarding path.
    entry->p2pActive = (sfuRTPSender_ == nullptr || !sfuActive_);

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

  // Prime P2P RTT as well: the first SR/RR exchange can be delayed.
  enableRTCPWarmup(entry->p2pRTPSender);
  entry->p2pRtcpWarmupActive = true;
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

  // A new participant can arrive while SFU is temporarily disabled from an earlier
  // all-P2P decision. Ensure there is an active fallback path for this link immediately.
  if (sfuRTPSender_ && !sfuActive_ && !entry->p2pActive)
  {
    Logger::getLogger()->printWarning(this, "Re-enabling SFU for newly added link",
                                      {"CNAME", "SFU SSRC"},
                                      {cname, QString::number(ssrc)});
    pendingSfuActive_ = true;
    applySfuState(true);
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


void HybridFilter::enableRTCPWarmup(std::shared_ptr<UvgRTPSender> sender)
{
  const int computedSessionKbps = static_cast<int>((getHWManager()->getEncoderBitrate(sender->inputType()) / (1.0 - TRANSMISSION_OVERHEAD)) / 1000);

  if (computedSessionKbps < RTCP_WARMUP_SESSION_BANDWIDTH_KBPS)
  {
    sender->setTemporarySessionBandwidthKbps(RTCP_WARMUP_SESSION_BANDWIDTH_KBPS);
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
      const bool firstSample = entry->p2pRTT.empty();

      entry->p2pRTT.push_back(rtt);
      if (entry->p2pRTT.size() > MAX_RTT_MEASUREMENTS)
      {
        entry->p2pRTT.pop_front();
      }

      // As soon as we have our first P2P RTT sample, revert any temporary
      // RTCP warmup override for this link.
      if (entry->p2pRtcpWarmupActive && entry->p2pRTPSender)
      {
        entry->p2pRtcpWarmupActive = false;

        if (entry->p2pActive)
        {
          // clear temporary start boost for faster rtt and return to normal rtcp traffic
          entry->p2pRTPSender->clearTemporarySessionBandwidth();
        }
        else
        {
          // lower rtcp for inactive sessions
          entry->p2pRTPSender->setTemporarySessionBandwidthKbps(DUMMY_SESSION_BANDWIDTH_KBPS);
        }

        Logger::getLogger()->printNormal(this, "Disabled P2P RTCP warmup",
                                         {"CNAME", "Active"},
                                         {pair.first, entry->p2pActive ? "true" : "false"});
      }

      // Check if link change is beneficial, speeds up adoption
      if (firstSample)
      {
        if (!entry->sfuRTT.empty() && sfuRTPSender_ && sfuActive_)
        {
          triggerReEvaluation_ = true;
          Logger::getLogger()->printNormal(this, "Triggered immediate re-evaluation after first P2P RTT sample",
                                           {"CNAME", "P2P_SSRC", "SFU_SSRC"},
                                           {pair.first, QString::number(entry->p2pSSRC), QString::number(entry->sfuSSRC)});
        }
        else
        {
          Logger::getLogger()->printNormal(this, "No re-evaluation after first P2P RTT sample received",
                          {"CNAME", "Type", "RTT"}, {pair.first, "P2P", QString::number(rtt)});
        }
      }

      foundSSRC = true;
      break;
    }
    
    if (entry->sfuSSRC == ssrc)
    {
      const bool firstSample = entry->sfuRTT.empty();

      entry->sfuRTT.push_back(rtt);
      if (entry->sfuRTT.size() > MAX_RTT_MEASUREMENTS)
      {
        entry->sfuRTT.pop_front();
      }

      // Check if link change is beneficial, speeds up adoption
      if (firstSample)
      { 
        if (!entry->p2pRTT.empty() && entry->p2pRTPSender && sfuRTPSender_)
        {
          triggerReEvaluation_ = true;
          Logger::getLogger()->printNormal(this, "Triggered immediate re-evaluation after first SFU RTT sample",
                                           {"CNAME", "P2P_SSRC", "SFU_SSRC"},
                                           {pair.first, QString::number(entry->p2pSSRC), QString::number(entry->sfuSSRC)});
        }
        else
        {        
          Logger::getLogger()->printNormal(this, "No re-evaluation after first SFU RTT sample",
                                           {"CNAME","Type","RTT"}, {pair.first, "SFU", QString::number(rtt)});
        }
      }

      foundSSRC = true;
      break;
    }
  }

  if (!foundSSRC) // error
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
  // slaves are typically less bandwidth heavy protocols like audio
  slaveMutex_.lock();
  slaves_.push_back(slave);
  slaveMutex_.unlock();
}


std::unique_ptr<Data> HybridFilter::recordEncodedFrameStatistics(std::unique_ptr<Data> input)
{
  if (!input)
    return input;

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

  return input;
}


void HybridFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    ++count_;
    if (count_ % EVALUATION_INTERVAL == 0)
    {
      triggerReEvaluation_ = true;
    }

    if (count_ % DUMMY_INTERVAL == 0)
    {
      if (hasAnyP2PLinks() && sfuRTPSender_ != nullptr)
      {
        sendDummies(input->rtpTimestamp); // to get latency measurements for inactive connections
      }
    }

    if (triggerReEvaluation_)
    {
      if (hasAnyP2PLinks() && sfuRTPSender_ != nullptr)
      {
        reEvaluateConnections(input->rtpTimestamp);
      }

      triggerReEvaluation_ = false;
    }

    // Check and execute switches BEFORE sending output to ensure connection state
    // matches the SFU's forwarding state at the exact same timestamp
    if (hasNextSwitchTimestamp_)
    {
      // Use RTP timestamp space comparisons to handle rollover.
      if (rtpTsAtOrAfter(input->rtpTimestamp, nextSwitchTimestamp_))
      {
        const int32_t delta = static_cast<int32_t>(input->rtpTimestamp - nextSwitchTimestamp_);
        Logger::getLogger()->printNormal(this, "Executing switches at RTP timestamp",
                                        {"Current", "Scheduled", "Delta"},
                                        {QString::number(input->rtpTimestamp), 
                                         QString::number(nextSwitchTimestamp_),
                                         QString::number(delta)});
        executeSwitches(input->rtpTimestamp);
      }
    }

    input = recordEncodedFrameStatistics(std::move(input));

    sendOutput(std::move(input));
    input = getInput();
  }
}


bool HybridFilter::rtpTsAtOrAfter(uint32_t t1, uint32_t t2) const
{
  // Return true if `t1` is equal to or later than `t2` in RTP timestamp space.
  return static_cast<int32_t>(t1 - t2) >= 0;
}


bool HybridFilter::rtpTsSoonerFrom(uint32_t now, uint32_t a, uint32_t b) const
{
  // Return true if `a` occurs sooner than `b`, measured from `now` in RTP timestamp space.
  // Assumes both timestamps are within < 2^31 ticks from `now` (true for our scheduling horizon).
  return static_cast<int32_t>(a - now) < static_cast<int32_t>(b - now);
}


void HybridFilter::clearOngoingSwitchState(const std::shared_ptr<LinkInfo>& link)
{
  if (!link)
    return;

  link->switchPhase = LinkInfo::SwitchPhase::None;
  link->switchTargetP2P = false;
  link->switchTimestamp = 0;
  link->switchProhabitionMs = 0;
}


uint64_t HybridFilter::calculateSwitchGuardWindowMs(const std::shared_ptr<LinkInfo>& link,
                                                    int syncPeriodFrames) const
{
  const double frameMs = (1000.0 * static_cast<double>(framerateDenominator_)) / 
                                   static_cast<double>(framerateNumerator_);
  const double syncLeadMs = std::max(1.0, static_cast<double>(syncPeriodFrames) * frameMs);

  double rttGuardMs = std::max(link->latestsP2PRtt, link->latestsSFURtt);

  return static_cast<uint64_t>(std::ceil(syncLeadMs + std::max(0.0, rttGuardMs)));
}


void HybridFilter::executeSwitches(uint32_t currentTimestamp)
{
  // Delayed switch of links, needs to be synchronized with sfu server.
  // Execute only those whose scheduled RTP timestamp has been reached.
  std::vector<std::shared_ptr<LinkInfo>> remaining;
  remaining.reserve(linksToSwitch_.size());

  uint32_t nextPendingTs = 0;
  bool hasNextPendingTs = false;

  for (const auto& link : linksToSwitch_)
  {
    if (link->switchPhase == LinkInfo::SwitchPhase::Scheduled)
    {
      if (rtpTsAtOrAfter(currentTimestamp, link->switchTimestamp))
      {
        if (link->switchTargetP2P) // switch SFU -> P2P
        {
          Logger::getLogger()->printNormal(this, "Switch from SFU to P2P for SSRC " +
                                            QString::number(link->sfuSSRC) + " to " + QString::number(link->p2pSSRC));

          link->p2pActive = true;
          if (link->p2pOutIndex >= 0)
          {
            setConnection(link->p2pOutIndex, true, link->p2pRTPSender);
          }
        }
        else // switch P2P -> SFU
        {
          Logger::getLogger()->printNormal(this, "Switch from P2P to SFU for SSRC " +
                                            QString::number(link->p2pSSRC) + " to " + QString::number(link->sfuSSRC));

          link->p2pActive = false;
          if (link->p2pOutIndex >= 0)
          {
            setConnection(link->p2pOutIndex, false, link->p2pRTPSender);
          }
        }

        // Keep switchPhase in AwaitingCompletion after execute until post-switch stabilization window expires.
        link->switchPhase = LinkInfo::SwitchPhase::AwaitingCompletion;
        link->switchTimestamp = 0;

        // wait a little bit more in case something is ongoing at receiver
        link->switchProhabitionMs = clockNowMs() + std::max(link->latestsP2PRtt, link->latestsSFURtt);
      }
      else
      {
        remaining.push_back(link);
        if (!hasNextPendingTs || rtpTsSoonerFrom(currentTimestamp, link->switchTimestamp, nextPendingTs))
        {
          nextPendingTs = link->switchTimestamp;
          hasNextPendingTs = true;
        }
      }
    }
  }

  linksToSwitch_.swap(remaining);
  hasNextSwitchTimestamp_ = hasNextPendingTs;
  if (hasNextPendingTs)
  {
    nextSwitchTimestamp_ = nextPendingTs;
  }
  else
  {
    nextSwitchTimestamp_ = 0;
  }

  // Apply SFU state decision from evaluation when all scheduled switches
  // have been executed. This avoids disabling SFU while a switch to SFU is
  // still pending.
  if (linksToSwitch_.empty())
  {
    applySfuState(pendingSfuActive_);
  }
}


bool HybridFilter::allowNewSwitchRequest(const std::shared_ptr<LinkInfo>& linkInfo, const QString& targetPath)
{
  if (!linkInfo)
    return false;

  if (linkInfo->switchPhase != LinkInfo::SwitchPhase::None)
  {
    const uint64_t nowMs = clockNowMs();
    if (linkInfo->switchProhabitionMs > 0 && nowMs >= linkInfo->switchProhabitionMs)
    {
      QString phase = "unknown";
      if (linkInfo->switchPhase == LinkInfo::SwitchPhase::Scheduled)
        phase = "scheduled";
      else if (linkInfo->switchPhase == LinkInfo::SwitchPhase::AwaitingCompletion)
        phase = "awaiting-completion";

      Logger::getLogger()->printWarning(this, "Clearing expired switch state",
                                        {"Target", "Phase", "P2P SSRC", "SFU SSRC"},
                                        {targetPath,
                                         phase,
                                         QString::number(linkInfo->p2pSSRC),
                                         QString::number(linkInfo->sfuSSRC)});
      clearOngoingSwitchState(linkInfo);
    }

    if (linkInfo->switchPhase != LinkInfo::SwitchPhase::None)
    {
      QString phase = "unknown";
      if (linkInfo->switchPhase == LinkInfo::SwitchPhase::Scheduled)
        phase = "scheduled";
      else if (linkInfo->switchPhase == LinkInfo::SwitchPhase::AwaitingCompletion)
        phase = "awaiting-completion";

      Logger::getLogger()->printWarning(this, "Link has an ongoing switch, skipping switch",
                                        {"Target", "Phase", "P2P SSRC", "SFU SSRC"},
                                        {targetPath,
                                        phase,
                                        QString::number(linkInfo->p2pSSRC),
                                        QString::number(linkInfo->sfuSSRC)});
      return false;
    }
  }

  return true;
}


void HybridFilter::markSwitchOngoing(const std::shared_ptr<LinkInfo>& linkInfo,
                                     uint32_t scheduledTimestamp,
                                     int syncPeriodFrames)
{
  if (!linkInfo)
    return;

  const uint64_t switchOngoingTimeoutMs = calculateSwitchGuardWindowMs(linkInfo, syncPeriodFrames);

  linkInfo->switchPhase = LinkInfo::SwitchPhase::Scheduled;
  linkInfo->switchTimestamp = scheduledTimestamp;
  // Use a conservative schedule lifetime to tolerate delayed frame arrival.
  linkInfo->switchProhabitionMs = clockNowMs() + (switchOngoingTimeoutMs * 2);
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

  const bool mustKeepSfu = hasAnyLinkNeedingSfu();
  const bool effectiveNeedSfu = (needSFU || mustKeepSfu);

  if (!needSFU && mustKeepSfu)
  {
    Logger::getLogger()->printWarning(this, "Keeping SFU enabled because at least one link still needs SFU",
                                      {"Requested", "Effective"},
                                      {"disabled", "enabled"});
  }

  sfuActive_ = effectiveNeedSfu;
  setConnection(sfuOutIndex_, sfuActive_, sfuRTPSender_);
  Logger::getLogger()->printNormal(this, "Applying SFU state",
                                  "SFU State", sfuActive_ ? "enabled" : "disabled");
}


bool HybridFilter::hasAnyLinkNeedingSfu() const
{
  for (const auto& kv : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& entry = kv.second;
    if (!entry)
      continue;

    const bool hasSfu = (entry->sfuSSRC != 0);
    const bool hasActiveP2P = (entry->p2pRTPSender && entry->p2pActive);

    // If we have an SFU identity for the link but no active P2P path, SFU must stay on.
    if (hasSfu && !hasActiveP2P)
      return true;
  }

  return false;
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


double HybridFilter::calculateTotalSessionBandwidthBps(double fallbackBps) const
{
  int64_t payloadBandwidth = static_cast<int64_t>(getHWManager()->getEncoderBitrate(output_));
  for (const auto& slave : slaves_)
  {
    if (slave)
      payloadBandwidth += static_cast<int64_t>(slave->getBitrate());
  }

  const double denom = (1.0 - static_cast<double>(TRANSMISSION_OVERHEAD));
  if (payloadBandwidth > 0 && denom > 0.0)
  {
    return static_cast<double>(payloadBandwidth) / denom;
  }

  Logger::getLogger()->printWarning(this, "Using fallback session bandwidth",
                                   {"PayloadBps", "Overhead", "FallbackBps"},
                                   {QString::number(payloadBandwidth),
                                    QString::number(TRANSMISSION_OVERHEAD),
                                    QString::number(fallbackBps)});
  return fallbackBps;
}


double HybridFilter::calculateMinSfuOneWayLatencySec() const
{
  double minOneWayLatencySec = 0.0;
  bool hasLatencySample = false;

  for (const auto& kv : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& e = kv.second;
    if (e && e->latestsSFURtt >= 0.0)
    {
      const double oneWayLatencySec = (e->latestsSFURtt / 2.0) / 1000.0;
      if (!hasLatencySample)
      {
        minOneWayLatencySec = oneWayLatencySec;
        hasLatencySample = true;
      }
      else
      {
        minOneWayLatencySec = std::min(minOneWayLatencySec, oneWayLatencySec);
      }
    }
  }

  if (hasLatencySample)
  {
    return minOneWayLatencySec;
  }

  return 0.0;
}


void HybridFilter::reEvaluateConnections(uint32_t currentTimestamp)
{
  constexpr double kNoBandwidthFallbackBps = 0.0;
  const int64_t availableBandwidth = static_cast<int64_t>(settingValue(SettingsKey::sipUpBandwidth));

  if (availableBandwidth <= 0)
  {
    Logger::getLogger()->printError(this, "Connection bandwidth is zero or negative, cannot re-evaluate connections");
    return;
  }

  // Convert payload bitrate to an estimated total (payload + RTP/RTCP overhead).
  // Our overhead constant represents a fraction of total bandwidth, so:
  //   payload = total * (1 - overhead)  =>  total = payload / (1 - overhead)
  const int64_t linkBandwidth = static_cast<int64_t>(
      std::ceil(calculateTotalSessionBandwidthBps(kNoBandwidthFallbackBps)));

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
  pendingSfuActive_ = true; // we need sfu if we cannot afford using only P2P connections
}


void HybridFilter::delayedSwitchToP2P(std::shared_ptr<LinkInfo> linkInfo, uint32_t currentTimestamp)
{
  if (!linkInfo)
    return;

  if (!allowNewSwitchRequest(linkInfo, "P2P"))
    return;
  
  const int syncPeriodFrames = calculateSyncPeriodInFrames();

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
    // Calculate future timestamp for when the switch will occur
    uint32_t futureTimestamp = updateVideoRtpTimestamp(currentTimestamp, framerateNumerator_, framerateDenominator_, syncPeriodFrames);
    linkInfo->switchTargetP2P = true;
    markSwitchOngoing(linkInfo, futureTimestamp, syncPeriodFrames);
    if (!hasNextSwitchTimestamp_ || rtpTsSoonerFrom(currentTimestamp, futureTimestamp, nextSwitchTimestamp_))
    {
      nextSwitchTimestamp_ = futureTimestamp;
      hasNextSwitchTimestamp_ = true;
    }

    Logger::getLogger()->printNormal(this, "Scheduling delayed switch to P2P connection",
                                    {"SFU SSRC","P2P SSRC", "Current TS", "Future TS", "SyncFrames"},
                                    {QString::number(linkInfo->sfuSSRC), QString::number(linkInfo->p2pSSRC),
                                     QString::number(currentTimestamp), QString::number(futureTimestamp),
                                     QString::number(syncPeriodFrames)});

    // sync with sfu server
    if (sfuRTPSender_)
    {
      sfuRTPSender_->stopForwarding(linkInfo->sfuSSRC, futureTimestamp);
    }
    // Keep track of links with scheduled switch timestamps.
    if (std::find(linksToSwitch_.begin(), linksToSwitch_.end(), linkInfo) == linksToSwitch_.end())
      linksToSwitch_.push_back(linkInfo);
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

  if (!allowNewSwitchRequest(linkInfo, "SFU"))
    return;

  if (!sfuRTPSender_ || linkInfo->sfuSSRC == 0)
  {
    Logger::getLogger()->printWarning(this, "Cannot switch to SFU: SFU RTP sender not available");
    return;
  }

  const int syncPeriodFrames = calculateSyncPeriodInFrames();

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
    uint32_t futureTimestamp = updateVideoRtpTimestamp(currentTimestamp, framerateNumerator_, framerateDenominator_, syncPeriodFrames);
    linkInfo->switchTargetP2P = false;
    markSwitchOngoing(linkInfo, futureTimestamp, syncPeriodFrames);
    if (!hasNextSwitchTimestamp_ || rtpTsSoonerFrom(currentTimestamp, futureTimestamp, nextSwitchTimestamp_))
    {
      nextSwitchTimestamp_ = futureTimestamp;
      hasNextSwitchTimestamp_ = true;
    }

    Logger::getLogger()->printNormal(this, "Scheduling delayed switch to SFU connection",
                                    {"P2P SSRC","SFU SSRC", "Current TS", "Future TS", "SFU Active", "SyncFrames"},
                                    {QString::number(linkInfo->p2pSSRC), QString::number(linkInfo->sfuSSRC),
                                     QString::number(currentTimestamp), QString::number(futureTimestamp),
                                     sfuActive_ ? "true" : "false",
                                     QString::number(syncPeriodFrames)});

    // Ensure SFU stays enabled while we are switching participants back to it.
    pendingSfuActive_ = true;

    // sync with sfu server
    sfuRTPSender_->startForwarding(linkInfo->sfuSSRC, futureTimestamp);

    // Keep track of links with scheduled switch timestamps.
    if (std::find(linksToSwitch_.begin(), linksToSwitch_.end(), linkInfo) == linksToSwitch_.end())
    {
      linksToSwitch_.push_back(linkInfo);
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Link is already on SFU (P2P not active), no need to switch");
  }
}


int HybridFilter::calculateSyncPeriodInFrames() const
{
  constexpr double kRtcpTotalBwFraction = 0.05; // RTCP is typically 5% of session bandwidth
  constexpr double kRtcpSenderBwFraction = 0.25;
  constexpr double kRtcpRcvrBwFraction = 0.75;
  constexpr double kRtcpCompensation = 2.71828 - 1.5;
  constexpr double kRtcpRandomMaxFactor = 1.5 / kRtcpCompensation;

  const int fpsNum = std::max(1, framerateNumerator_);
  const int fpsDen = std::max(1, framerateDenominator_);
  const double fps = static_cast<double>(fpsNum) / static_cast<double>(fpsDen);

  // Estimate uplink session bandwidth from current encoder+slave bitrates,
  // including transmission overhead.
  const double totalBandwidthBps = calculateTotalSessionBandwidthBps(DEFAULT_SYNC_SESSION_BANDWIDTH_BPS);
  const double sessionKbps = std::max(1.0, totalBandwidthBps / 1000.0);

  // RTCP bandwidth as kbps.
  double rtcpBwKbps = std::max(0.1, sessionKbps * kRtcpTotalBwFraction);

  // Model sender-side interval (we_sent = true).
  const int members = std::max(2, static_cast<int>(cnameToLinks_.size() + 1));
  const int senders = 1;
  int n = members;
  if (senders <= static_cast<int>(std::floor(static_cast<double>(members) * kRtcpSenderBwFraction)))
  {
    rtcpBwKbps *= kRtcpSenderBwFraction;
    n = std::max(1, senders);
  }
  else
  {
    rtcpBwKbps *= kRtcpRcvrBwFraction;
    n = std::max(1, members - senders);
  }

  const double rtcpBwOctetsPerSec = std::max(1.0, (1000.0 * rtcpBwKbps) / 8.0);

  double intervalSec = (RTCP_AVG_PACKET_SIZE_OCTETS * static_cast<double>(n)) / rtcpBwOctetsPerSec;
  if (intervalSec < 5.0)
    intervalSec = 5.0;

  // Use worst-case randomization multiplier and budget for multiple RTCP
  // cycles (initial schedule + retries/queueing under load).
  const double worstSenderIntervalSec = intervalSec * kRtcpRandomMaxFactor;
  double controlCycles = 2.0;
  if (cnameToLinks_.size() >= 10)
    controlCycles = 3.0;
  const double controlLeadSec = worstSenderIntervalSec * controlCycles;

  // Add measured one-way path estimate from current SFU RTT samples.
  const double minOneWayLatencySec = calculateMinSfuOneWayLatencySec();

  const double leadSec = controlLeadSec + minOneWayLatencySec + SYNC_CUSHION_SECONDS;
  const int dynamicFrames = static_cast<int>(std::ceil(leadSec * fps));
  const int syncFrames = std::min(MAX_SYNC_PERIOD_IN_FRAMES, dynamicFrames);

  return syncFrames;
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
      // Keep current path when RTT difference is within hysteresis deadband.
      if (entry->latestsP2PRtt <= 0.0 || entry->latestsSFURtt <= 0.0)
      {
        // no valid measurements, keep existing state
        Logger::getLogger()->printWarning(this, "No valid RTT measurements for links, keeping existing state",
                                         {"CNAME", "P2P RTT", "SFU RTT"}, {pair.first,
                                          QString::number(entry->latestsP2PRtt) + " ms",
                                          QString::number(entry->latestsSFURtt) + " ms"});

        // Since we keep existing state, count the link based on the current active path.
        if (entry->p2pActive)
        {
          ++p2pLinks;
        }
        else
        {
          needSFU = true;
        }
      }
      else if (!entry->p2pActive && entry->latestsP2PRtt + RTT_SWITCH_THRESHOLD_MS < entry->latestsSFURtt)
      {
        ++p2pLinks;
        Logger::getLogger()->printNormal(this, "Switch link to P2P",
                                        {"CNAME", "Expected latency reduction", "SSRC"},
                                        {pair.first, QString::number(entry->latestsSFURtt - entry->latestsP2PRtt) + " ms",
                                         QString::number(entry->p2pSSRC)});
        delayedSwitchToP2P(entry, currentTimestamp);
      }
      else if (entry->p2pActive && entry->latestsSFURtt + RTT_SWITCH_THRESHOLD_MS < entry->latestsP2PRtt)
      {
        Logger::getLogger()->printNormal(this, "Switch link to SFU",
                                         {"CNAME", "Expected latency increase", "SSRC"},
                                         {pair.first, QString::number(entry->latestsP2PRtt - entry->latestsSFURtt) + " ms",
                                          QString::number(entry->sfuSSRC)});
        delayedSwitchToSFU(entry, currentTimestamp);

        needSFU = true;
      }
      else
      {
        // Inside deadband: keep current path to avoid oscillation.
        if (entry->p2pActive)
          ++p2pLinks;
        else
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
  // (RTT benefit, link). Benefit is SFU RTT minus P2P RTT (higher means bigger win for P2P).
  std::vector<std::pair<double, std::shared_ptr<LinkInfo>>> rankedConnections;

  for (auto& pair : cnameToLinks_)
  {
    const std::shared_ptr<LinkInfo>& entry = pair.second;
    if (!entry)
      continue;

    if (entry->p2pRTPSender && entry->sfuSSRC != 0)
    {
      const double rttBenefit = entry->latestsSFURtt - entry->latestsP2PRtt;
      rankedConnections.push_back(std::make_pair(rttBenefit, entry));
    }
  }

  // Sort by RTT benefit (ascending), then iterate from the end (descending).
  std::sort(rankedConnections.begin(), rankedConnections.end());

  int p2pLinks = 0;
  // Enable P2P connections until we reach the limit
  const int p2pLimit = std::max(0, maxP2PConnections);
  const size_t total = rankedConnections.size();
  for (size_t order = 0; order < total; ++order)
  {
    const size_t idx = total - 1 - order; // descending by benefit
    const double rttBenefit = rankedConnections[idx].first;
    const std::shared_ptr<LinkInfo>& link = rankedConnections[idx].second;

    const bool withinBandwidthBudget = (static_cast<int>(order) < p2pLimit);

    if (link->p2pActive)
    {
      // Keep active P2P unless SFU is clearly better by threshold.
      const bool keepP2P = withinBandwidthBudget && (rttBenefit >= -RTT_SWITCH_THRESHOLD_MS);
      if (keepP2P)
      {
        ++p2pLinks;
      }
      else
      {
        delayedSwitchToSFU(link, currentTimestamp);
      }
    }
    else
    {
      // Promote SFU->P2P only when there is clear P2P advantage.
      const bool shouldSwitchToP2P = withinBandwidthBudget && (rttBenefit > RTT_SWITCH_THRESHOLD_MS);
      if (shouldSwitchToP2P)
      {
        delayedSwitchToP2P(link, currentTimestamp);
        ++p2pLinks;
      }
    }
  }

  Logger::getLogger()->printNormal(this, "Partial-bandwidth evaluation",
                                  {"Connection bandwidth", "maxP2PConnections", "P2P Links"},
                                  {QString::number(connectionBandwidth), QString::number(maxP2PConnections),
                                   QString::number(p2pLinks) + "/" + QString::number(cnameToLinks_.size())});
}
