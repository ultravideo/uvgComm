#pragma once

#include "filter.h"
#include <H265VideoRTPSink.hh>


class RTPSinkFilter : public Filter, public MediaSink
{
public:
  RTPSinkFilter(QString id, StatisticsInterface* stats, UsageEnvironment& env, DataType type);

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

protected:
  void process();

  private:

  virtual Boolean continuePlaying();

  u_int8_t* fReceiveBuffer;

  DataType type_;
};
