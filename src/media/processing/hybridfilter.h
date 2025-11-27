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
  unsigned int p2pOutIndex;
  std::shared_ptr<UvgRTPSender> p2pRTPSender;

  std::deque<double> p2pRTT;
  double latestsP2PRtt = 0.0;

  // while the sfu link is common, we still
  // have a separate ssrc for stats tracking
  uint32_t sfuSSRC;
  std::deque<double> sfuRTT;
  double latestsSFURtt = 0.0;
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

public slots:

  void recordRTT(uint32_t ssrc, double rtt);

protected:
  virtual void process() override;

private:

  void setConnection(int index, bool status);

  double averageRTT(const std::deque<double>& samples) const;

  void reEvaluateConnections();

  void sendDummies();

  void fullBandwidthEvaluation();

  void rankedBandwidthEvaluation(const int maxP2PConnections, int connectionBandwidth);

  void delayedSwitchToP2P(std::shared_ptr<LinkInfo> linkInfo);
  void delayedSwitchToSFU(std::shared_ptr<LinkInfo> linkInfo);

  QMutex slaveMutex_;
  std::vector<std::shared_ptr<HybridSlaveFilter>> slaves_;

  std::unordered_map<QString, std::shared_ptr<LinkInfo>> cnameToLinks_;

  bool triggerReEvaluation_;

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

  int nextSwitch_ = -1;
  std::vector<std::shared_ptr<LinkInfo>> linksToSwitch_;
};
