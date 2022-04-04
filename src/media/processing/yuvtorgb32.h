#pragma once
#include "filter.h"

// converts the YUV420 video frame to and RGB32 frame. May use optimizations.

class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(QString id, StatisticsInterface* stats,
             std::shared_ptr<ResourceAllocator> hwResources);

  virtual void updateSettings();

protected:
  void process();

private:
  int threadCount_;
};

