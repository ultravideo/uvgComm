#pragma once

#include "filter.h"

#include <QString>

class StatisticsInterface;
class ResourceAllocator;
class HybridSlaveFilter;

enum LinkType
{
  LINK_SFU,
  LINK_P2P
};

class HybridFilter : public Filter
{
public:
  HybridFilter(QString id, StatisticsInterface *stats,
               std::shared_ptr<ResourceAllocator> hwResources);

  ~HybridFilter() override;

  void addSlave(std::shared_ptr<HybridSlaveFilter> slave);

  void addLink(LinkType type);

protected:
  virtual void process() override;

private:

  void setConnection(int index, bool status);

  QMutex slaveMutex_;
  std::vector<std::shared_ptr<HybridSlaveFilter>> slaves_;

  std::vector<unsigned int> p2pLinks_;
  unsigned int sfuLink_;

  int count_;
};
