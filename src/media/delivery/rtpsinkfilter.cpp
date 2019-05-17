#include <QDebug>
#include <cstdio>

#include "statisticsinterface.h"
#include "rtpsinkfilter.h"
#include "common.h"

const uint32_t BUFFER_SIZE = 10 * 65536;

#define RTP_HEADER_SIZE 2
#define FU_HEADER_SIZE  1

static void __receiveHook(void *arg, RTPGeneric::GenericFrame *frame)
{
  if (arg && frame)
  {
    static_cast<RTPSinkFilter *>(arg)->receiveHook(frame);
  }
}

RTPSinkFilter::RTPSinkFilter(QString id, StatisticsInterface *stats, DataType type, QString media, RTPReader *reader):
  Filter(id, "RTP_Sink_" + media, stats, NONE, type),
  type_(type),
  addStartCodes_(true),
  reader_(reader)
{
  qDebug() << "\n\n\nCREATING RTPSinkFilter!!\n\n\n";
  getStats()->addFilter(getName(), (uint64_t)currentThreadId());

  reader_->installReceiveHook(this, __receiveHook);
}

RTPSinkFilter::~RTPSinkFilter()
{
}

void RTPSinkFilter::uninit()
{
}

void RTPSinkFilter::process()
{
}

void RTPSinkFilter::start()
{
}

void RTPSinkFilter::receiveHook(RTPGeneric::GenericFrame *frame)
{
  RTPGeneric::GenericFrame *f = frame;

  getStats()->addReceivePacket(frame->dataLen);

  if (addStartCodes_ && type_ == HEVCVIDEO)
  {
    // fragmentation unit
    if ((frame->data[0] >> 1) == 49)
    {
      bool firstFrag = frame->data[2] & 0x80;
      bool lastFrag  = frame->data[2] & 0x40;

      if (firstFrag && lastFrag)
      {
        qDebug() << "PEER ERROR: invalid combination of S and E bits!";
        return;
      }

      // TODO should this be part of rtp lib instead?

      if (firstFrag)
      {
        // create new NAL header for fragmented frame
        uint8_t NALHeader[2] = { 0 };
        NALHeader[0] = (frame->data[0] & 0x81) | ((frame->data[2] & 0x3f) << 1);
        NALHeader[1] = frame->data[1];

        // discard RTP and FU headers
        frame->data    += (RTP_HEADER_SIZE + FU_HEADER_SIZE);
        frame->dataLen -= (RTP_HEADER_SIZE + FU_HEADER_SIZE);

        memcpy(fragFrame_,                     NALHeader,   sizeof(NALHeader));
        memcpy(&fragFrame_[sizeof(NALHeader)], frame->data, frame->dataLen);

        fragSize_ = sizeof(NALHeader) + frame->dataLen;

        // start of the fragmented frame, no reason to continue processing just yet
        // so return early. The frame is processed when the last fragmentation unit is received
        return;
      }
      else
      {
        // discard RTP and FU headers
        frame->data    += (RTP_HEADER_SIZE + FU_HEADER_SIZE);
        frame->dataLen -= (RTP_HEADER_SIZE + FU_HEADER_SIZE);

        // append this fragmentation unit at the end of frag buffer
        memcpy(&fragFrame_[fragSize_], frame->data, frame->dataLen);
        fragSize_ += frame->dataLen;

        // we haven't received the last fragmentation unit so exit early
        if (!lastFrag)
        {
          return;
        }

        // we've received the full frame
        f->data    = fragFrame_;
        f->dataLen = fragSize_;
        fragSize_  = 0;
      }
    }

    f->dataLen += 4;
  }

  Data *received_picture = new Data;
  received_picture->data_size = f->dataLen;
  received_picture->type = type_;
  received_picture->data = std::unique_ptr<uchar[]>(new uchar[received_picture->data_size]);
  received_picture->width = 0; // not know at this point. Decoder tells the correct resolution
  received_picture->height = 0;
  received_picture->framerate = 0;
  received_picture->source = REMOTE;

  // TODO: This copying should be done in separate thread as in
  // framedsource if we want to receive 4K with less powerful thread (like in Xeon)
  if (addStartCodes_ && type_ == HEVCVIDEO)
  {
    memcpy(received_picture->data.get() + 4, f->data, received_picture->data_size - 4);
    received_picture->data[0] = 0;
    received_picture->data[1] = 0;
    received_picture->data[2] = 0;
    received_picture->data[3] = 1;
  }
  else
  {
    memcpy(received_picture->data.get(), f->data, received_picture->data_size);
  }

  std::unique_ptr<Data> rp( received_picture );
  sendOutput(std::move(rp));
}
