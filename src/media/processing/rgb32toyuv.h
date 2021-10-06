#pragma once
#include "filter.h"

// converts the RGB32 video frame to and YUV420 frame. May use optimizations.


class RGB32toYUV : public Filter
{
public:
  RGB32toYUV(QString id, StatisticsInterface* stats,
             std::shared_ptr<HWResourceManager> hwResources);

  virtual void updateSettings();

protected:

  // flips input
  void process();

private:

  int threadCount_;
};
