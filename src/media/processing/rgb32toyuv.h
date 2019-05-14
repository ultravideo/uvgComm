#pragma once
#include "filter.h"

// converts the RGB32 video frame to and YUV420 frame. May use optimizations.


class RGB32toYUV : public Filter
{
public:
  RGB32toYUV(QString id, StatisticsInterface* stats, uint32_t peer);

protected:

  // flips input
  void process();

private:

  bool sse_;

};
