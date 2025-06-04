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
  LinkType type;
  uint32_t ssrc;
  bool active;
  unsigned int outIndex; // same for all sfu, different for p2p
  std::shared_ptr<UvgRTPSender> rtpSender;

  std::deque<double> rttSamples;
  double latestsRtt = 0.0;
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

  double averageRTT(const std::shared_ptr<LinkInfo>& link) const;

  void reEvaluateConnections();

  void sendDummies();

  void fullBandwidthEvaluation();

  void delayedSwitchToP2P(std::shared_ptr<LinkInfo> p2p, std::shared_ptr<LinkInfo> sfu);
  void delayedSwitchToSFU(std::shared_ptr<LinkInfo> p2p, std::shared_ptr<LinkInfo> sfu);

  QMutex slaveMutex_;
  std::vector<std::shared_ptr<HybridSlaveFilter>> slaves_;

  struct LinkPair
  {
    std::shared_ptr<LinkInfo> p2p = nullptr;
    std::shared_ptr<LinkInfo> sfu = nullptr;
  };

  std::unordered_map<QString, LinkPair> cnameToLinks_;

  bool triggerReEvaluation_;

  int sfuIndex_;

  uint64_t count_;

  int nextSwitch_ = -1;
  std::vector<LinkPair> linksToSwitch_;
};
