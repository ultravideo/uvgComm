#pragma once


#include "rtpstreamer.h"
#include "filter.h"

#include <FramedSource.hh>
#include <QMutex>

class FramedSourceFilter : public FramedSource, public Filter
{
public:
  FramedSourceFilter(UsageEnvironment &env, DataType type);

  virtual void doGetNextFrame();

  virtual Boolean isH265VideoStreamFramer() const
  {
    return type_ == HEVCVIDEO;
  }

  virtual bool isInputFilter() const
  {
    return false;
  }
  virtual bool isOutputFilter() const
  {
    return true;
  }

protected:
  void process();

private:

  DataType type_;

};
