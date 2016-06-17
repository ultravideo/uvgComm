#pragma once

#include <FramedSource.hh>

class FramedHEVCSource : public FramedSource
{
public:
  FramedHEVCSource(UsageEnvironment &env);

  virtual void doGetNextFrame();

  virtual Boolean isH265VideoStreamFramer() const
  {
    return true;
  }

};

