#include "rtpsinkfilter.h"

#include <QDebug>


const uint32_t BUFFER_SIZE = 65536;



RTPSinkFilter::RTPSinkFilter(UsageEnvironment& env, Groupsock *gs, unsigned char pf):
  Filter(), H265VideoRTPSink(*createNew(env, gs, pf))
{
  fReceiveBuffer = new u_int8_t[BUFFER_SIZE];
}

RTPSinkFilter::~RTPSinkFilter()
{}

void RTPSinkFilter::afterGettingFrame(void* clientData,
                              unsigned frameSize,
                              unsigned numTruncatedBytes,
                              struct timeval presentationTime,
                              unsigned durationInMicroseconds)
{
  Q_ASSERT(clientData);
  RTPSinkFilter* sink = (RTPSinkFilter*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes,
                          presentationTime, durationInMicroseconds);
}

void RTPSinkFilter::afterGettingFrame(unsigned frameSize,
                       unsigned numTruncatedBytes,
                       struct timeval presentationTime,
                       unsigned durationInMicroseconds)
{
  qDebug() << "Received HEVC frame. Size: " << frameSize;
}


void RTPSinkFilter::process()
{}


Boolean RTPSinkFilter::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}
