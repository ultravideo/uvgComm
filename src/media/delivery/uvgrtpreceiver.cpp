#include "uvgrtpreceiver.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "logger.h"
#include "common.h"

#include <QtConcurrent>
#include <QFuture>
#include <QDebug>

#include <QCoreApplication>
#include <QThread>

#include <functional>

#define RTP_HEADER_SIZE 2
#define FU_HEADER_SIZE  1

static void __receiveHook(void *arg, uvg_rtp::frame::rtp_frame *frame)
{
  if (arg && frame)
  {
    static_cast<UvgRTPReceiver *>(arg)->receiveHook(frame);
  }
}

UvgRTPReceiver::UvgRTPReceiver(uint32_t sessionID, QString id,
                               StatisticsInterface *stats,
                               std::shared_ptr<ResourceAllocator> hwResources,
                               DataType type, QString media,
                               uint32_t localSSRC,  uint32_t remoteSSRC,
                               uvgrtp::media_stream *stream, bool runZRTP)
:Filter(id, "RTP Receiver " + media, stats, hwResources, DT_NONE, type),
  discardUntilIntra_(false),
  lastSeq_(0),
  sessionID_(sessionID),
  localSSRC_(localSSRC),
  remoteSSRC_(remoteSSRC),
  stream_(stream),
  lastSEITime_(0)
{
  Logger::getLogger()->printNormal(this, "Initializing uvgRTP receiver",
                                  {"LocalSSRC", "Remote SSRC", "Receiver type"},
                                  {QString::number(localSSRC_),
                                   QString::number(remoteSSRC_),
                                   datatypeToString(output_)});

  {
    std::lock_guard<std::mutex> g(streamMutex_);
    if (stream_)
    {
      stream_->install_receive_hook(this, __receiveHook);
      if (stream_->get_rtcp())
        stream_->get_rtcp()->install_sender_hook(std::bind(&UvgRTPReceiver::processRTCPSenderReport,
                                                          this, std::placeholders::_1 ));
    }
  }

  // Apply session bandwidth after RTCP has been initialized so RTCP scheduling
  // (both SR and RR/SDES) uses the intended value even for recv-only sessions.
  updateSessionBandwidth();

  // Keep uvgRTP session bandwidth in sync when participant count changes.
  if (getHWManager())
  {
    QObject::connect(getHWManager().get(), &ResourceAllocator::participantsChanged,
                     this, [this](int)
                     {
                       updateSessionBandwidth();
                     },
                     Qt::QueuedConnection);
  }

  if (runZRTP)
  {
    futureRes_ =
        QtConcurrent::run([=](uvgrtp::media_stream *ms)
                          {
                            return ms->start_zrtp();
                          },
                          stream_);
  }
}


void UvgRTPReceiver::updateSessionBandwidth()
{
  if (!alive_.load())
    return;

  int payloadBitrateBps = 0;
  if (getHWManager())
    payloadBitrateBps = getHWManager()->getEncoderBitrate(outputType());

  // Convert to total session bandwidth (kbps) and leave room for overhead.
  int totalSessionBitrateKbps = 0;
  if (getHWManager())
  {
    int totalBps = getHWManager()->getStreamBandwidthUsage(outputType());
    totalSessionBitrateKbps = std::max(10, totalBps / 1000);
  }
  else
  {
    totalSessionBitrateKbps = std::max(10, static_cast<int>((payloadBitrateBps / (1.0 - 0.10)) / 1000));
  }

  if (totalSessionBitrateKbps == lastSessionBandwidthKbps_)
    return;

  std::lock_guard<std::mutex> g(streamMutex_);
  if (!stream_)
  {
    if (!loggedNoStreamForBandwidth_.exchange(true))
    {
      Logger::getLogger()->printWarning(this, "Skipping RCC_SESSION_BANDWIDTH: no stream",
                                       {"SessionID", "Type"},
                                       {QString::number(sessionID_), datatypeToString(outputType())});
    }
    return;
  }

  loggedNoStreamForBandwidth_.store(false);

  Logger::getLogger()->printNormal(this, "Applying RCC_SESSION_BANDWIDTH",
                                  {"SessionID", "SSRC", "Kbps", "Type"},
                                  {QString::number(sessionID_),
                                   QString::number(stream_->get_ssrc()),
                                   QString::number(totalSessionBitrateKbps),
                                   datatypeToString(outputType())});

  stream_->configure_ctx(RCC_SESSION_BANDWIDTH, totalSessionBitrateKbps);
  lastSessionBandwidthKbps_ = totalSessionBitrateKbps;
}


UvgRTPReceiver::~UvgRTPReceiver()
{
  uninit();
}


void UvgRTPReceiver::stop()
{
  uninit();
  Filter::stop();
}


void UvgRTPReceiver::uninit()
{
  // mark as not alive so callbacks can bail out early
  alive_.store(false);

  // If ZRTP handshake thread is running, wait for it to finish so we don't
  // race with stream destruction.
  if (futureRes_.isRunning())
  {
    Logger::getLogger()->printWarning(this, "Waiting for ZRTP to finish in destructor");
    futureRes_.waitForFinished();
  }

  std::lock_guard<std::mutex> g(streamMutex_);
  if (stream_)
  {
    // Remove all RTCP hooks
    if (stream_->get_rtcp())
    {
      stream_->get_rtcp()->remove_all_hooks();
    }

    // Install a nullptr receive hook to clear any installed receive hook.
    stream_->install_receive_hook(nullptr, nullptr);
    // Null out our reference to make it safe for concurrent checks.
    stream_ = nullptr;
  }
}


void UvgRTPReceiver::process()
{}

void UvgRTPReceiver::receiveHook(uvg_rtp::frame::rtp_frame *frame)
{
  // If we're tearing down, don't process incoming frames.
  if (!alive_.load())
    return;

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
    Logger::getLogger()->printError(this, "Got a packet with wrong SSRC",
                                    {"Expected SSRC", "Packet SSRC", "Receiver Type"},
                                    {QString::number(remoteSSRC_),
                                     QString::number(frame->header.ssrc),
                                     datatypeToString(output_)});
    return;
  }

  lastSeq_ = frame->header.seq;

  std::unique_ptr<Data> received_picture = initializeData(output_, DS_REMOTE);

  if (!received_picture)
    return;

  // Extract RTP timestamp from packet header
  received_picture->rtpTimestamp = frame->header.timestamp;

  // Record SSRC so downstream filters can identify the packet source
  received_picture->ssrc = frame->header.ssrc;
  /*
  // Check for SEI NAL anywhere in the RTP payload (handles co-packed NALs)
  if (output_ == DT_HEVCVIDEO && frame->payload_len >= 33)
  {
    for (size_t pos = 0; pos + 33 <= (size_t)frame->payload_len; ++pos)
    {
      if (frame->payload[pos] == 0x00 && frame->payload[pos+1] == 0x00 &&
          frame->payload[pos+2] == 0x00 && frame->payload[pos+3] == 0x01)
      {
        uint8_t nalType = (frame->payload[pos+4] >> 1) & 0x3F;
        if (nalType == 39 || nalType == 40)
        {
          int64_t creationTimestamp = 0;
          for (int i = (int)(pos + 32); i >= (int)(pos + 25); --i)
          {
            creationTimestamp = (creationTimestamp << 8) | (frame->payload[i] & 0xFF);
          }
          lastSEITime_ = creationTimestamp;
          return;
        }
      }
    }
  }
*/

  if(lastSEITime_ != 0)
  {
    received_picture->creationTimestamp = lastSEITime_;
  }
  else
  {
    received_picture->creationTimestamp = 0;
  }
  received_picture->presentationTimestamp = clockNowMs();

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

    received_picture->data[0] = 0;
    received_picture->data[1] = 0;
    received_picture->data[2] = 0;
    received_picture->data[3] = 1;

    memcpy(received_picture->data.get() + 4, frame->payload, received_picture->data_size - 4);
  }
  else
  {
    // We use the memory provided by uvgRTP so we don't have to copy the data.
    // The RCE_H26X_PREPEND_SC flag set in delivery adds NAL start codes to frames so we don't have to
    received_picture->data_size = (uint32_t)frame->payload_len;
    received_picture->data = std::unique_ptr<uchar[]>(frame->payload);
    frame->payload = nullptr;    // avoid memory deletion
  }

  (void)uvgrtp::frame::dealloc_frame(frame);

  if (!alive_.load())
  {
    return;
  }

  sendOutput(std::move(received_picture));
}


void UvgRTPReceiver::processRTCPSenderReport(std::unique_ptr<uvgrtp::frame::rtcp_sender_report> sr)
{
  // If we're tearing down or stream gone, bail out early.
  std::lock_guard<std::mutex> g(streamMutex_);
  if (!alive_.load() || !stream_)
    return;

  //TODO: Record the newest NTP and RTP timestamps from sender report

  //uint64_t msw = sr->sender_info.ntp_msw;
  //uint64_t ntp = msw << 32 | sr->sender_info.ntp_lsw;

  uint32_t ourSSRC = stream_->get_ssrc();

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

      getStats()->addRTCPPacket(sessionID_, "", type,
                                block.fraction,
                                block.lost,
                                block.last_seq,
                                block.jitter);
    }
  }
}
