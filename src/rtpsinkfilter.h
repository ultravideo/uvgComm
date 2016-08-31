#pragma once

#include "filter.h"
#include <H265VideoRTPSink.hh>


class RTPSinkFilter : public Filter, public MediaSink
{
public:
  RTPSinkFilter(StatisticsInterface* stats, UsageEnvironment& env);

  virtual ~RTPSinkFilter();

  static void afterGettingFrame(void* clientData,
                                unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);

  void afterGettingFrame(unsigned frameSize,
                         unsigned numTruncatedBytes,
                         struct timeval presentationTime,
                         unsigned durationInMicroseconds);

  virtual bool isInputFilter() const
  {
    return true;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

protected:
  void process();

  private:

  virtual Boolean continuePlaying();

  u_int8_t* fReceiveBuffer;
};
