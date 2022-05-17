#include "uvgrtpsender.h"

#include "statisticsinterface.h"
#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

#include <functional>

UvgRTPSender::UvgRTPSender(uint32_t sessionID, QString id, StatisticsInterface *stats,
                           std::shared_ptr<ResourceAllocator> hwResources,
                           DataType type, QString media,
                           QFuture<uvg_rtp::media_stream *> mstream):
  Filter(id, "RTP Sender " + media, stats, hwResources, type, DT_NONE),
  mstream_(nullptr),
  sessionID_(sessionID),
  rtpFlags_(RTP_NO_FLAGS)
{
  UvgRTPSender::updateSettings();

  connect(&watcher_, &QFutureWatcher<uvg_rtp::media_stream *>::finished,
          [this]()
          {
            mstream_ = watcher_.result(); // TODO: This can crash in sending
            if (!mstream_)
            {
              emit zrtpFailure(sessionID_);
            }
            else
            {
              mstream_->get_rtcp()->install_receiver_hook(std::bind(&UvgRTPSender::processRTCPReceiverReport,
                                                                    this, std::placeholders::_1 ));
            }
          });

  watcher_.setFuture(mstream);
}


UvgRTPSender::~UvgRTPSender()
{}


void UvgRTPSender::updateSettings()
{
  // called in case we later decide to add some settings to filter
  Filter::updateSettings();

  if (input_ == DT_HEVCVIDEO)
  {
    uint32_t vps   = settingValue(SettingsKey::videoVPS);
    uint16_t intra = (uint16_t)settingValue(SettingsKey::videoIntra);

    if (settingEnabled(SettingsKey::videoSlices))
    {
      rtpFlags_ |= RTP_SLICE;
    }
    else
    {
      // the number of frames between parameter sets
      maxBufferSize_ = vps * intra;
      rtpFlags_ &= ~RTP_SLICE;
    }

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "Updated buffersize", 
                                    {"Size"}, {QString::number(maxBufferSize_)});
  }
}


void UvgRTPSender::process()
{
  if (!mstream_)
    return;

  rtp_error_t ret = RTP_OK;
  std::unique_ptr<Data> input = getInput();

  // TODO: For HEVC, make sure that the first frame we send is intra
  while (input)
  {
    ret = mstream_->push_frame(std::move(input->data), input->data_size, rtpFlags_);

    if (ret != RTP_OK)
    {
      Logger::getLogger()->printDebug(DEBUG_ERROR, this,  "Failed to send data", 
                                      { "Error" }, { QString::number(ret) });
    }

    getStats()->addSendPacket(input->data_size);

    input = getInput();
  }
}


void UvgRTPSender::processRTCPReceiverReport(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report> rr)
{
  uint32_t ourSSRC = mstream_->get_ssrc();

  for (auto& block : rr->report_blocks)
  {
    if (block.ssrc == ourSSRC)
    {
      QString type = "Other";
      if (isVideo(inputType()))
      {
        type = "Video";
      }
      else if (isAudio(inputType()))
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
