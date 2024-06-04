#include "uvgrtpsender.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
#include <QtConcurrent>
#include <QFuture>

#include <functional>

UvgRTPSender::UvgRTPSender(uint32_t sessionID, QString id, StatisticsInterface *stats,
                           std::shared_ptr<ResourceAllocator> hwResources,
                           DataType type, QString media,
                           uvg_rtp::media_stream* mstream,
                           uint32_t localSSRC, uint32_t remoteSSRC):
  Filter(id, "RTP Sender " + media, stats, hwResources, type, DT_NONE, false),
  mstream_(mstream),
  sessionID_(sessionID),
  rtpFlags_(RTP_NO_FLAGS),
  framerateNumerator_(0),
  framerateDenominator_(0),
  localSSRC_(localSSRC),
  remoteSSRC_(remoteSSRC)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initializing uvgRTP sender",
                                  {"LocalSSRC", "Remote SSRC", "Sender type"},
                                  {QString::number(localSSRC_),
                                   QString::number(remoteSSRC_),
                                   datatypeToString(input_)});

  UvgRTPSender::updateSettings();

  if (localSSRC_ != 0)
  {
    mstream_->configure_ctx(RCC_SSRC, localSSRC_);
  }
  if (remoteSSRC_ != 0)
  {
    mstream_->configure_ctx(RCC_REMOTE_SSRC, remoteSSRC_);
  }


  if (settingEnabled(SettingsKey::sipSRTP))
  {
    futureRes_ =
        QtConcurrent::run([=](uvg_rtp::media_stream *ms)
                          {
                            return ms->start_zrtp();
                          },
                          mstream_);
  }
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
    maxBufferSize_ = vps * intra;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "Updated buffersize",
                                    {"Size"}, {QString::number(maxBufferSize_)});

    framerateNumerator_ = settingValue(SettingsKey::videoFramerateNumerator);
    framerateDenominator_ = settingValue(SettingsKey::videoFramerateDenominator);

    if (mstream_ &&
            dataFormat_ == RTP_FORMAT_H264 ||
            dataFormat_ == RTP_FORMAT_H265 ||
            dataFormat_ == RTP_FORMAT_H266)
    {
      mstream_->configure_ctx(RCC_FPS_NUMERATOR, framerateNumerator_);
      mstream_->configure_ctx(RCC_FPS_DENOMINATOR, framerateDenominator_);
    }
  }
}


void UvgRTPSender::process()
{
  if (!mstream_)
    return;

  if (futureRes_.isRunning())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Waiting for ZRTP to finish");
    return;
  }

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

      getStats()->addRTCPPacket(sessionID_, type,
                                block.fraction,
                                block.lost,
                                block.last_seq,
                                block.jitter);
    }
  }
}
