
#include <cstdio>

#include "statisticsinterface.h"
#include "uvgrtpreceiver.h"

#include "common.h"
#include "logger.h"

#include <QDateTime>
#include <QDebug>

#define RTP_HEADER_SIZE 2
#define FU_HEADER_SIZE  1

static void __receiveHook(void *arg, uvg_rtp::frame::rtp_frame *frame)
{
  if (arg && frame)
  {
    static_cast<UvgRTPReceiver *>(arg)->receiveHook(frame);
  }
}

UvgRTPReceiver::UvgRTPReceiver(uint32_t sessionID, QString id, StatisticsInterface *stats,
                               std::shared_ptr<ResourceAllocator> hwResources,
                               DataType type, QString media, QFuture<uvg_rtp::media_stream *> stream):
  Filter(id, "RTP Receiver " + media, stats, hwResources, DT_NONE, type),
  gotSeq_(false),
  discardUntilIntra_(false),
  lastSeq_(0),
  sessionID_(sessionID)
{
  connect(&watcher_, &QFutureWatcher<uvg_rtp::media_stream *>::finished,
          [this]()
          {
            if (!watcher_.result())
              emit zrtpFailure(sessionID_);
            else
              watcher_.result()->install_receive_hook(this, __receiveHook);
          });

  watcher_.setFuture(stream);
}

UvgRTPReceiver::~UvgRTPReceiver()
{}

void UvgRTPReceiver::process()
{}

void UvgRTPReceiver::receiveHook(uvg_rtp::frame::rtp_frame *frame)
{
  Q_ASSERT(frame && frame->payload != nullptr);

  if (frame == nullptr ||
      frame->payload == nullptr ||
      frame->payload_len == 0)
  {
    Logger::getLogger()->printProgramError(this,  "Received a nullptr frame from uvgRTP");
    return;
  }

  if (output_ == DT_HEVCVIDEO && gotSeq_)
  {
/*
 *  TODO: Enable this once uvgRTP has correctly implemented sequence numbers
    if (shouldDiscard(frame->header.seq, frame->payload))
    {
      lastSeq_ = frame->header.seq;
      (void)uvg_rtp::frame::dealloc_frame(frame);
      return;
    }
    */
  }

  gotSeq_ = true;
  lastSeq_ = frame->header.seq;

  std::unique_ptr<Data> received_picture = initializeData(output_, DS_REMOTE);

  // TODO: Get this info from RTP
  received_picture->presentationTime = QDateTime::currentMSecsSinceEpoch();

  // check if the uvgRTP added start code and if not, add it ourselves
  if (output_ == DT_HEVCVIDEO &&
      ((frame->payload[0]) != 0 ||
      ( frame->payload[1]) != 0 ||
      ( frame->payload[2]) != 0 ||
      ( frame->payload[3]) != 1))
  {
    Logger::getLogger()->printWarning(this, "uvgRTP did not add the start code. Please use newer version"
                                            " of uvgRTP and make sure RCE_H26X_PREPEND_SC flag is used");
    received_picture->data_size = (uint32_t)frame->payload_len + 4;
    received_picture->data = std::unique_ptr<uchar[]>(new uchar[received_picture->data_size]);

    memcpy(received_picture->data.get() + 4, frame->payload, received_picture->data_size - 4);

    received_picture->data[0] = 0;
    received_picture->data[1] = 0;
    received_picture->data[2] = 0;
    received_picture->data[3] = 1;
  }
  else
  {
    // We use the memory provided by uvgRTP so we don't have to copy the data.
    // The RCE_H26X_PREPEND_SC flag set in delivery adds NAL start codes to frames so we don't have to
    received_picture->data_size = (uint32_t)frame->payload_len;
    received_picture->data = std::unique_ptr<uchar[]>(frame->payload);
    frame->payload = nullptr;    // avoid memory deletion
  }

  (void)uvg_rtp::frame::dealloc_frame(frame);
  sendOutput(std::move(received_picture));
}

bool UvgRTPReceiver::shouldDiscard(uint16_t frameSeq, uint8_t* payload)
{
  if (gotSeq_)
  {
    if ((lastSeq_ == UINT16_MAX - 1 && frameSeq != 0) ||
        frameSeq > lastSeq_ + 1)
    {
      // We have detected that there are frames missing after last intra

      if (isHEVCIntra(payload))
      {
        discardUntilIntra_ = false;
        return false;
      }

      Logger::getLogger()->printDebug(DEBUG_WARNING, this,
                                      "Missing frame detected. Discarding if inter",
                                      {"Last Seq", "Current Seq"},
                                      {QString::number(lastSeq_), QString::number(frameSeq)});

      discardUntilIntra_ = true;

      return isHEVCInter(payload);
    }
    else if (discardUntilIntra_ && isHEVCInter(payload))
    {
      Logger::getLogger()->printWarning(this,  "Discarding frame because no intra"
                                               " has arrived after last missing frame",
                                        "Seq", QString::number(frameSeq));
      return true;
    }
    else if (isHEVCIntra(payload))
    {
      discardUntilIntra_ = false;
    }
  }

  return false;
}
