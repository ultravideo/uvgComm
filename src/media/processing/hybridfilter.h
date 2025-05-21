#pragma once

#include "filter.h"

#include <QString>

class StatisticsInterface;
class ResourceAllocator;

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

  void addLink(LinkType type);

protected:
  virtual void process() override;

private:

  int count_;
};
