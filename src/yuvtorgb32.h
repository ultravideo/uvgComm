#pragma once

#include "filter.h"


class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(StatisticsInterface* stats);

protected:

  void process();

};

