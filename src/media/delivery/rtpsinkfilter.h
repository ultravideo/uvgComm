#pragma once
#include "media/processing/filter.h"
#include "filter.h"
#include "../rtplib/src/reader.hh"

// Receives RTP packets and sends them forward in filter graph.

class RTPSinkFilter : public Filter
{
public:
  RTPSinkFilter(QString id, StatisticsInterface *stats, DataType type, QString media, RTPReader *reader);
  ~RTPSinkFilter();

  void receiveHook(RTPGeneric::GenericFrame *frame);

  void uninit();
  void start();

protected:
  void process();

private:
  // TODO why this must be static

  DataType type_;
  bool addStartCodes_;

  RTPReader *reader_;

  uint8_t fragFrame_[16384];

  std::vector<RTPGeneric::GenericFrame *> fragBuf_;
  size_t fragSize_;
};

#if 0
/* #include <H265VideoRTPSink.hh> */

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
#endif
