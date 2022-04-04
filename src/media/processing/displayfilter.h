#pragma once
#include "filter.h"

class VideoInterface;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(QString id, StatisticsInterface* stats,
                std::shared_ptr<ResourceAllocator> hwResources,
                QList<VideoInterface*> widgets, uint32_t peer);
  ~DisplayFilter();

  void setHorizontalMirroring(bool status)
  {
    horizontalMirroring_ = status;
  }

protected:
  void process();

private:

  bool horizontalMirroring_;

  // Owned by Conference view
  QList<VideoInterface*> widgets_;

  uint32_t sessionID_;
};
