#include "rtpsinkfilter.h"

#include <QDebug>


const uint32_t BUFFER_SIZE = 65536;



RTPSinkFilter::RTPSinkFilter(UsageEnvironment& env):
  Filter(), MediaSink(env)
{
  fReceiveBuffer = new u_int8_t[BUFFER_SIZE];
}

RTPSinkFilter::~RTPSinkFilter()
{
  delete fReceiveBuffer;
  fReceiveBuffer = 0;
}

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
  qDebug() << "Received HEVC frame. Size: " << frameSize
           << ", truncated: " << numTruncatedBytes;

  Q_ASSERT(numTruncatedBytes == 0);

  Data *received_picture = new Data;
  received_picture->data_size = frameSize;
  received_picture->type = HEVCVIDEO;
  received_picture->data = std::unique_ptr<uchar[]>(new uchar[received_picture->data_size]);
  received_picture->width = 0;
  received_picture->height = 0;
  received_picture->presentationTime = presentationTime;
  memcpy(received_picture->data.get(), fReceiveBuffer, received_picture->data_size);
  std::unique_ptr<Data> rp( received_picture );
  sendOutput(std::move(rp));

  continuePlaying();
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
