#pragma once
#include "filter.h"

class VideoInterface;

class DisplayFilter : public Filter
{
public:
  DisplayFilter(QString id, StatisticsInterface* stats,
                std::shared_ptr<ResourceAllocator> hwResources,
                VideoInterface *widget, uint32_t peer);
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
  VideoInterface* widget_;

  uint32_t sessionID_;
};
