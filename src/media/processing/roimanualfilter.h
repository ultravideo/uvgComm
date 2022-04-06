#pragma once

#include "filter.h"

class VideoInterface;

class ROIManualFilter : public Filter
{
public:
  ROIManualFilter(QString id, StatisticsInterface* stats,
                  std::shared_ptr<ResourceAllocator> hwResources,
                  VideoInterface* roiInterface);

  virtual bool init();

  virtual void updateSettings();

protected:
  void process();

private:
  VideoInterface* roiSurface_;

  int qp_;
};
