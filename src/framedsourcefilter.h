#pragma once


#include "rtpstreamer.h"
#include "filter.h"

#include <FramedSource.hh>
#include <QMutex>

class FramedSourceFilter : public FramedSource, public Filter
{
public:
  FramedSourceFilter(StatisticsInterface* stats,
                     UsageEnvironment &env, DataType type);

  // called by live555. Takes a sample from input and schedules it to be sent.
  virtual void doGetNextFrame();

  virtual Boolean isH265VideoStreamFramer() const
  {
    return type_ == HEVCVIDEO;
  }

protected:
  void process();

private:

  DataType type_;

};
