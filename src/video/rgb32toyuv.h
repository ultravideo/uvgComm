#pragma once
#include "filter.h"

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
