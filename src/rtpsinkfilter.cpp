#include "rtpsinkfilter.h"

#include <statisticsinterface.h>
#include "common.h"

#include <RTPInterface.hh>

#include <QDebug>

const uint32_t BUFFER_SIZE = 10*65536;

RTPSinkFilter::RTPSinkFilter(QString id, StatisticsInterface *stats, UsageEnvironment& env, DataType type, QString media):
  Filter(id, "RTP_Sink_" + media, stats, NONE, type),
  MediaSink(env),
  type_(type),
  addStartCodes_(false)
{
  fReceiveBuffer = new u_int8_t[BUFFER_SIZE];
  stats_->addFilter(name_, (uint64_t)currentThreadId());
}

RTPSinkFilter::~RTPSinkFilter()
{}

void RTPSinkFilter::uninit()
{
  stopPlaying();
  while (fSource  != nullptr)
  {
    qSleep(1);
  }
  qDebug() << "Deleting RTPSink:" << name_ << "type:" << type_;
  delete fReceiveBuffer;
  fReceiveBuffer = nullptr;
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

  Q_UNUSED(durationInMicroseconds);

  // TODO increase buffer for large frames
  Q_ASSERT(numTruncatedBytes == 0);

  stats_->addReceivePacket(frameSize);

  if(addStartCodes_ && type_ == HEVCVIDEO)
  {
    //qDebug() << "Received HEVC frame. Size: " << frameSize
    //         << ", truncated: " << numTruncatedBytes;
    frameSize += 4;
  }

  Data *received_picture = new Data;
  received_picture->data_size = frameSize;
  received_picture->type = type_;
  received_picture->data = std::unique_ptr<uchar[]>(new uchar[received_picture->data_size]);
  received_picture->width = 0; // not know at this point. Decoder tells the correct resolution
  received_picture->height = 0;
  received_picture->framerate = 0;
  received_picture->presentationTime = presentationTime;
  received_picture->source = REMOTE;

  // TODO: This copying should be done in separate thread as in
  // framedsource if we want to receive 4K with less powerful thread (like in Xeon)
  if(addStartCodes_ && type_ == HEVCVIDEO)
  {
    memcpy(received_picture->data.get() + 4, fReceiveBuffer, received_picture->data_size - 4);
    received_picture->data[0] = 0;
    received_picture->data[1] = 0;
    received_picture->data[2] = 0;
    received_picture->data[3] = 1;
  }
  else
  {
    memcpy(received_picture->data.get(), fReceiveBuffer, received_picture->data_size);
  }

  std::unique_ptr<Data> rp( received_picture );
  sendOutput(std::move(rp));

  continuePlaying();
}

void RTPSinkFilter::process()
{}

Boolean RTPSinkFilter::continuePlaying() {
  if (fSource == nullptr) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}
