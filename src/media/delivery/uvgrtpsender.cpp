#include "uvgrtpsender.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
#include <QtConcurrent>
#include <QFuture>


UvgRTPSender::UvgRTPSender(uint32_t sessionID, QString id,
                           StatisticsInterface *stats,
                           std::shared_ptr<ResourceAllocator> hwResources,
                           DataType type, QString media,
                           uint32_t localSSRC, uint32_t remoteSSRC,
                           uvgrtp::media_stream* stream, bool runZRTP)
  :Filter(id, "RTP Sender " + media, stats, hwResources, type, DT_NONE, false),
  stream_(stream),
  sessionID_(sessionID),
  rtpFlags_(RTP_NO_FLAGS),
  framerateNumerator_(0),
  framerateDenominator_(0)
{
  Q_ASSERT(stream_);

  Logger::getLogger()->printNormal(this, "Initializing uvgRTP sender",
                                  {"LocalSSRC", "Remote SSRC", "Sender type"},
                                  {QString::number(localSSRC),
                                   QString::number(remoteSSRC),
                                   datatypeToString(input_)});

  UvgRTPSender::updateSettings();

  // Update uvgRTP session bandwidth whenever participant count changes.
  // This ensures RTCP intervals track allocation changes immediately.
  if (getHWManager())
  {
    QObject::connect(getHWManager().get(), &ResourceAllocator::participantsChanged,
                     this, [this](int)
                     {
                       updateSessionBandwidth();
                     },
                     Qt::QueuedConnection);
  }

  if (input_ == DT_HEVCVIDEO)
    awaitingKeyframe_ = true;

  std::function<void(uint32_t, uint32_t, double)> f = std::bind(&UvgRTPSender::rtt, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

  {
    std::lock_guard<std::mutex> g(streamMutex_);
    if (stream_ && stream_->get_rtcp())
      stream_->get_rtcp()->install_roundtrip_time_hook(f);
  }

  if (runZRTP)
  {
    futureRes_ =
        QtConcurrent::run([=](uvgrtp::media_stream *ms)
                          {
                            return ms->start_zrtp();
                          }, stream_);
  }
}


void UvgRTPSender::updateSessionBandwidth()
{
  if (!alive_.load())
    return;

  // ResourceAllocator returns bitrate in bits/sec; uvgRTP expects session bandwidth in kbps.
  int targetBitrateBps = 0;
  if (getHWManager())
    targetBitrateBps = getHWManager()->getEncoderBitrate(inputType());

  int targetKbps = std::max(10, targetBitrateBps / 1000);
  if (targetKbps == lastSessionBandwidthKbps_)
    return;

  std::lock_guard<std::mutex> g(streamMutex_);
  if (!stream_)
    return;

  stream_->configure_ctx(RCC_SESSION_BANDWIDTH, targetKbps);
  lastSessionBandwidthKbps_ = targetKbps;
}


UvgRTPSender::~UvgRTPSender()
{
  uninit();
}


void UvgRTPSender::stop()
{
  uninit();
  Filter::stop();
}


void UvgRTPSender::uninit()
{
  // mark as not alive so callbacks / process() can bail out early
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
    // Null out our reference to make it safe for concurrent checks.
    stream_ = nullptr;
  }
}

void UvgRTPSender::startForwarding(uint32_t remoteSSRC, uint32_t futureTimestamp)
{
  sendAPP(remoteSSRC, futureTimestamp, "STRT", 0);
}


void UvgRTPSender::stopForwarding(uint32_t remoteSSRC, uint32_t futureTimestamp)
{
  sendAPP(remoteSSRC, futureTimestamp, "STOP", 1);
}


void UvgRTPSender::sendAPP(uint32_t remoteSSRC, uint32_t futureTimestamp, const char* name, uint8_t subtype)
{
  if (futureRes_.isRunning())
  {
    Logger::getLogger()->printWarning(this, "Waiting for ZRTP to finish");
    // Block until ZRTP handshake finishes instead of using ad-hoc sleeps.
    // This keeps behavior deterministic and avoids timing races.
    futureRes_.waitForFinished();
  }

  Logger::getLogger()->printNormal(this, "Scheduling RTCP APP packet",
                                  {"Future Timestamp", "Name"},
                                  {QString::number(futureTimestamp), QString(name)});

  // Allocate and populate 8-byte payload: [SSRC (4 bytes)] [Timestamp (4 bytes)]
  uint32_t netSSRC = htonl(remoteSSRC);
  uint32_t netTimestamp = htonl(futureTimestamp);

  uint8_t payload[8];
  memcpy(payload, &netSSRC, 4);
  memcpy(payload + 4, &netTimestamp, 4);

  std::lock_guard<std::mutex> g(streamMutex_);
  if (stream_ && stream_->get_rtcp())
  {
    rtp_error_t result = stream_->get_rtcp()->send_app_packet(name, subtype, sizeof(payload), payload);
    if (result != RTP_OK)
    {
      Logger::getLogger()->printWarning(this, "Failed to send RTCP APP STOP packet");
    }
  }

}


void UvgRTPSender::rtt(uint32_t localSSRC, uint32_t remoteSSRC, double time)
{
  //Logger::getLogger()->printNormal(this, "RTT received",
  //                                {"Time (ms)", "SSRC"}, {QString::number(time, 'f', 2), QString::number(remoteSSRC)});

  if (time < 0 || time > 5000) // we assume max 5 seconds
  {
    Logger::getLogger()->printError("Hybrid", "Invalid RTT value: " + QString::number(time));
    return;
  }

  emit rttReceived(remoteSSRC, time);
}


void UvgRTPSender::updateSettings()
{
  // called in case we later decide to add some settings to filter
  Filter::updateSettings();

  // Also refresh uvgRTP session bandwidth so RTCP intervals track current allocation.
  updateSessionBandwidth();

  if (input_ == DT_HEVCVIDEO)
  {
    uint32_t vps   = settingValue(SettingsKey::videoVPS);
    uint16_t intra = (uint16_t)settingValue(SettingsKey::videoIntra);
    maxBufferSize_ = vps * intra;
    Logger::getLogger()->printNormal(this,  "Updated buffersize",
                                    {"Size"}, {QString::number(maxBufferSize_)});

    if (settingValue(SettingsKey::videoFileEnabled))
    {
      framerateNumerator_ = settingValue(SettingsKey::videoFileFramerate);
      framerateDenominator_ = 1;
    }
    else
    {
      framerateNumerator_ = settingValue(SettingsKey::videoFramerateNumerator);
      framerateDenominator_ = settingValue(SettingsKey::videoFramerateDenominator);
    }

    std::lock_guard<std::mutex> g(streamMutex_);
    if (stream_ &&
        dataFormat_ == RTP_FORMAT_H264 ||
        dataFormat_ == RTP_FORMAT_H265 ||
        dataFormat_ == RTP_FORMAT_H266)
    {
      stream_->configure_ctx(RCC_FPS_NUMERATOR, framerateNumerator_);
      stream_->configure_ctx(RCC_FPS_DENOMINATOR, framerateDenominator_);
    }
  }
}


void UvgRTPSender::process()
{
  if (!alive_.load())
    return;

  if (!stream_)
    return;

  if (futureRes_.isRunning())
  {
    Logger::getLogger()->printWarning(this, "Waiting for ZRTP to finish");
    return;
  }

  rtp_error_t ret = RTP_OK;
  std::unique_ptr<Data> input = getInput();

  // TODO: For HEVC, make sure that the first frame we send is intra
  while (input)
  {
    streamMutex_.lock();
    if (stream_)
    {
      if (input->type == DT_HEVCVIDEO && awaitingKeyframe_)
      {
        if (input->vInfo && input->vInfo->keyframe)
        {
          awaitingKeyframe_ = false;
          // Forward the keyframe below
        }
        else
        {
          // Drop this frame; wait for keyframe
          input = getInput();
          streamMutex_.unlock();
          continue;
        }
      }

      if (input->rtpTimestamp != 0)
      {
        ret = stream_->push_frame(std::move(input->data), input->data_size, input->rtpTimestamp, rtpFlags_);
      }
      else
      {
        Logger::getLogger()->printWarning(this, "No RTP timestamp available");
        ret = stream_->push_frame(std::move(input->data), input->data_size, rtpFlags_);
      }
    }
    streamMutex_.unlock();

    if (ret != RTP_OK)
    {
      Logger::getLogger()->printError(this,  "Failed to send data", 
                                      { "Error" }, { QString::number(ret) });
    }

    getStats()->addSendPacket(input->data_size);

    input = getInput();
  }
}


void UvgRTPSender::processRTCPReceiverReport(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report> rr)
{
  std::lock_guard<std::mutex> g(streamMutex_);
  if (!alive_.load() || !stream_)
    return;

  uint32_t ourSSRC = stream_->get_ssrc();

  for (auto& block : rr->report_blocks)
  {
    if (block.ssrc == ourSSRC)
    {
      getHWManager()->addRTCPReport(sessionID_, inputType(), block.lost, block.jitter);

      QString type = "Other";
      if (isVideo(inputType()))
      {
        type = "Video";
      }
      else if (isAudio(inputType()))
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
