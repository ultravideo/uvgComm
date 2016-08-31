#pragma once

#include "filter.h"


class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(StatisticsInterface* stats);


  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

protected:

  void process();

};

