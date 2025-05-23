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
  bool active;
  uint32_t ssrc;
  unsigned int outIndex;
  std::shared_ptr<UvgRTPSender> rtpSender;
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

protected:
  virtual void process() override;

private:

  void setConnection(int index, bool status);

  QMutex slaveMutex_;
  std::vector<std::shared_ptr<HybridSlaveFilter>> slaves_;

  struct LinkPair
  {
    std::shared_ptr<LinkInfo> p2p = nullptr;
    std::shared_ptr<LinkInfo> sfu = nullptr;
  };

  std::unordered_map<QString, LinkPair> cnameToLinks_;

  bool triggerReEvaluation_;
};
