#pragma once

#include "filter.h"


class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32();


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

