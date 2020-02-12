#include <QDebug>
#include <cstdio>

#include "statisticsinterface.h"
#include "kvzrtpreceiver.h"
#include "common.h"

#define RTP_HEADER_SIZE 2
#define FU_HEADER_SIZE  1

static void __receiveHook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
  if (arg && frame)
  {
    static_cast<KvzRTPReceiver *>(arg)->receiveHook(frame);
  }
}

KvzRTPReceiver::KvzRTPReceiver(QString id, StatisticsInterface *stats, DataType type,
                               QString media, kvz_rtp::media_stream *mstream):
  Filter(id, "RTP_Sink_" + media, stats, NONE, type),
  type_(type),
  addStartCodes_(true),
  mstream_(mstream)
{
  getStats()->addFilter(getName(), (uint64_t)currentThreadId());

  mstream_->install_receive_hook(this, __receiveHook);
}

KvzRTPReceiver::~KvzRTPReceiver()
{
}

void KvzRTPReceiver::process()
{
}

void KvzRTPReceiver::start()
{
}

void KvzRTPReceiver::receiveHook(kvz_rtp::frame::rtp_frame *frame)
{
  Q_ASSERT(frame && frame->payload != nullptr);

  if (frame == nullptr || frame->payload == nullptr)
  {
    printProgramError(this,  "Received a nullptr frame from kvzRTP");
    return;
  }

  // TODO: payload len?
  getStats()->addReceivePacket(frame->payload_len);

  if (addStartCodes_ && type_ == HEVCVIDEO)
  {
    frame->payload_len += 4;
  }

  Data *received_picture = new Data;
  received_picture->data_size = frame->payload_len;
  received_picture->type = type_;
  received_picture->data = std::unique_ptr<uchar[]>(new uchar[received_picture->data_size]);
  received_picture->width = 0; // not known at this point. Decoder tells the correct resolution
  received_picture->height = 0;
  received_picture->framerate = 0;
  received_picture->source = REMOTE;
  received_picture->presentationTime = {0,0}; // TODO

  // TODO: This copying should be done in separate thread as in
  // framedsource if we want to receive 4K with less powerful thread (like in Xeon)
  if (addStartCodes_ && type_ == HEVCVIDEO)
  {
    memcpy(received_picture->data.get() + 4, frame->payload, received_picture->data_size - 4);
    received_picture->data[0] = 0;
    received_picture->data[1] = 0;
    received_picture->data[2] = 0;
    received_picture->data[3] = 1;
  }
  else
  {
    memcpy(received_picture->data.get(), frame->payload, received_picture->data_size);
  }

  (void)kvz_rtp::frame::dealloc_frame(frame);
  std::unique_ptr<Data> rp( received_picture );
  sendOutput(std::move(rp));
}
