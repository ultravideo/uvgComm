#pragma once

#include "filter.h"

#include <QString>

class StatisticsInterface;
class ResourceAllocator;
class HybridSlaveFilter;
class UvgRTPSender;

enum LinkType
{
  LINK_SFU,
  LINK_P2P
};

struct LinkInfo
{
  uint32_t p2pSSRC;
  bool p2pActive;
  int p2pOutIndex;
  std::shared_ptr<UvgRTPSender> p2pRTPSender;

  // Temporarily boost RTCP rate until we get the first RTT sample.
  bool p2pRtcpWarmupActive = false;

  std::deque<double> p2pRTT;
  double latestsP2PRtt = 0.0;

  // while the sfu link is common, we still
  // have a separate ssrc for stats tracking
  uint32_t sfuSSRC;
  std::deque<double> sfuRTT;
  double latestsSFURtt = 0.0;
  // Pending switch requested for this link (used to schedule delayed, synchronized switches).
  enum PendingSwitchType { PendingNone = 0, PendingToP2P = 1, PendingToSFU = 2 };
  PendingSwitchType pendingSwitch = PendingNone;

  // RTP timestamp at which the pending switch should be executed.
  // 0 means not scheduled.
  uint32_t pendingSwitchTimestamp = 0;
};

class HybridFilter : public Filter
{
public:
  HybridFilter(QString id, StatisticsInterface *stats,
               std::shared_ptr<ResourceAllocator> hwResources);

  ~HybridFilter() override;

  void addSlave(std::shared_ptr<HybridSlaveFilter> slave);

  void addLink(LinkType type,
               uint32_t ssrc,
               const QString& cname,
               std::shared_ptr<UvgRTPSender> rtpSender);

  void updateSettings() override;

public slots:

  void recordRTT(uint32_t ssrc, double rtt);

protected:
  virtual void process() override;

private:
  bool hasAnyP2PLinks() const;

  void setLowRtcpMode(const std::shared_ptr<UvgRTPSender>& sender, bool enabled);

  void maybeEnableSfuRtcpWarmup();
  void maybeEnableP2pRtcpWarmup(const std::shared_ptr<LinkInfo>& link);

  void addP2PLink(std::shared_ptr<LinkInfo>& entry,
                  int outIdx,
                  uint32_t ssrc,
                  const QString& cname,
                  std::shared_ptr<UvgRTPSender> rtpSender);

  void addSFULink(std::shared_ptr<LinkInfo>& entry,
                  int outIdx,
                  uint32_t ssrc,
                  const QString& cname,
                  std::shared_ptr<UvgRTPSender> rtpSender);

  void executeSwitches(uint32_t currentTimestamp);

  void setConnection(int index, bool status, const std::shared_ptr<UvgRTPSender>& sender = nullptr);

  double averageRTT(const std::deque<double>& samples) const;

  double calculateTotalSessionBandwidthBps(double fallbackBps) const;
  double calculateMinSfuOneWayLatencySec() const;

  void reEvaluateConnections(uint32_t currentTimestamp);

  void sendDummies(uint32_t currentTimestamp);

  void fullBandwidthEvaluation(uint32_t currentTimestamp);

  void rankedBandwidthEvaluation(const int maxP2PConnections, int connectionBandwidth, uint32_t currentTimestamp);

  int calculateSyncPeriodInFrames() const;

  void delayedSwitchToP2P(std::shared_ptr<LinkInfo> linkInfo, uint32_t currentTimestamp);
  void delayedSwitchToSFU(std::shared_ptr<LinkInfo> linkInfo, uint32_t currentTimestamp);

  QMutex slaveMutex_;
  std::vector<std::shared_ptr<HybridSlaveFilter>> slaves_;

  std::unordered_map<QString, std::shared_ptr<LinkInfo>> cnameToLinks_;

  bool triggerReEvaluation_;

  // Number of registered P2P senders (maintained on add/remove).
  unsigned int p2pLinkCount_ = 0;

  bool sfuActive_;
  unsigned int sfuOutIndex_;
  std::shared_ptr<UvgRTPSender> sfuRTPSender_;

  // Desired SFU state computed during evaluation. Actual SFU activation
  // is applied later when switches are executed to avoid gaps.
  bool pendingSfuActive_ = false;

  // Apply the SFU state. If `immediate` is true the change is applied
  // right away; otherwise it is recorded as pending.
  void applySfuState(bool needSFU);

  uint64_t count_;

  uint32_t nextSwitchTimestamp_ = 0;
  std::vector<std::shared_ptr<LinkInfo>> linksToSwitch_;

  // Framerate for calculating future RTP timestamps
  int32_t framerateNumerator_;
  int32_t framerateDenominator_;

  // Cached setting value; refreshed in updateSettings() to avoid per-frame lookups.
  int32_t sipTimestampInterval_ = 0;

  // Startup helper: temporarily increase SFU RTCP rate to obtain the first RTT
  // sample sooner, then revert to the computed bandwidth.
  bool sfuRtcpWarmupActive_ = false;
};
