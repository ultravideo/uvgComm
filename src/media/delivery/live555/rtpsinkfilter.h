#pragma once
#include "media/processing/filter.h"

#include <H265VideoRTPSink.hh>

// Receives RTP packets and sends them forward in filter graph.

class RTPSinkFilter : public Filter, public MediaSink
{
public:
  RTPSinkFilter(QString id, StatisticsInterface* stats, UsageEnvironment& env, DataType type, QString media);

  virtual ~RTPSinkFilter();

  void uninit();

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

  bool addStartCodes_;
};
