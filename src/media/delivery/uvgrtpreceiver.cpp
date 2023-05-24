#include "uvgrtpreceiver.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "common.h"
#include "logger.h"

#include <QDateTime>
#include <QDebug>

#include <functional>
#include <cstdio>

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
                               DataType type, QString media, QFuture<uvg_rtp::media_stream *> stream,
                               uint32_t localSSRC, uint32_t remoteSSRC):
  Filter(id, "RTP Receiver " + media, stats, hwResources, DT_NONE, type),
  discardUntilIntra_(false),
  lastSeq_(0),
  sessionID_(sessionID),
  watcher_(),
  mstream_(nullptr),
  localSSRC_(localSSRC),
  remoteSSRC_(remoteSSRC)
{
  connect(&watcher_, &QFutureWatcher<uvg_rtp::media_stream *>::finished,
          [this]()
          {
            mstream_ = watcher_.result();
            if (!mstream_)
            {
              emit zrtpFailure(sessionID_);
            }
            else
            {
              mstream_->install_receive_hook(this, __receiveHook);
              mstream_->get_rtcp()->install_sender_hook(std::bind(&UvgRTPReceiver::processRTCPSenderReport,
                                                                           this, std::placeholders::_1 ));

              Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initializing uvgRTP receiver",
                                              {"Remote SSRC"}, {QString::number(remoteSSRC_)});

              if (localSSRC_ != 0)
              {
                mstream_->configure_ctx(RCC_SSRC, localSSRC_);
              }

              if (remoteSSRC_ != 0)
              {
                mstream_->configure_ctx(RCC_REMOTE_SSRC, remoteSSRC_);
              }
            }
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

  if (frame->header.ssrc != remoteSSRC_)
  {
    Logger::getLogger()->printError(this, "Got a packet with wrong SSRC", "SSRC",
                                    QString::number(frame->header.ssrc));
    return;
  }

  lastSeq_ = frame->header.seq;

  std::unique_ptr<Data> received_picture = initializeData(output_, DS_REMOTE);

  if (!received_picture)
    return;

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
void UvgRTPReceiver::processRTCPSenderReport(std::unique_ptr<uvgrtp::frame::rtcp_sender_report> sr)
{
  uint32_t ourSSRC = mstream_->get_ssrc();

  for (auto& block : sr->report_blocks)
  {
    if (block.ssrc == ourSSRC)
    {
      getHWManager()->addRTCPReport(sessionID_, outputType(), block.lost, block.jitter);

      QString type = "Other";
      if (isVideo(outputType()))
      {
        type = "Video";
      }
      else if (isAudio(outputType()))
      {
        type = "Audio";
      }

      getStats()->addRTCPPacket(sessionID_, type,
                                block.fraction,
                                block.lost,
                                block.last_seq,
                                block.jitter);
    }
  }
}
