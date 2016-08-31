#pragma once

#include "filter.h"


class RGB32toYUV : public Filter
{
public:
  RGB32toYUV(StatisticsInterface* stats);


  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

protected:

  // flips input
  void process();

};
