#include "uvgrtpreceiver.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "logger.h"

#include <QtConcurrent>
#include <QFuture>
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
                               DataType type, QString media, std::shared_ptr<UvgRTPStream> stream):
  Filter(id, "RTP Receiver " + media, stats, hwResources, DT_NONE, type),
  discardUntilIntra_(false),
  lastSeq_(0),
  sessionID_(sessionID),
  us_(stream)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initializing uvgRTP receiver",
                                  {"LocalSSRC", "Remote SSRC", "Receiver type"},
                                  {QString::number(us_->localSSRC),
                                   QString::number(us_->remoteSSRC),
                                   datatypeToString(output_)});

  us_->ms->install_receive_hook(this, __receiveHook);
  us_->ms->get_rtcp()->install_sender_hook(std::bind(&UvgRTPReceiver::processRTCPSenderReport,
                                                      this, std::placeholders::_1 ));
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

  if (frame->header.ssrc != us_->remoteSSRC)
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, this, "Got a packet with wrong SSRC",
                                    {"Expected SSRC", "Packet SSRC", "Receiver Type"},
                                    {QString::number(us_->remoteSSRC),
                                     QString::number(frame->header.ssrc),
                                     datatypeToString(output_)});
    return;
  }

  lastSeq_ = frame->header.seq;

  std::unique_ptr<Data> received_picture = initializeData(output_, DS_REMOTE);

  if (!received_picture)
    return;
  
  received_picture->creationTimestamp = QDateTime::currentMSecsSinceEpoch();
  received_picture->presentationTimestamp = received_picture->creationTimestamp;

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
  //TODO: Record the newest NTP and RTP timestamps from sender report

  //uint64_t msw = sr->sender_info.ntp_msw;
  //uint64_t ntp = msw << 32 | sr->sender_info.ntp_lsw;

  uint32_t ourSSRC = us_->ms->get_ssrc();

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
