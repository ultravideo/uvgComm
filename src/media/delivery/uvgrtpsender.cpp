#include <QSettings>

#include "uvgrtpsender.h"
#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"


UvgRTPSender::UvgRTPSender(uint32_t sessionID, QString id, StatisticsInterface *stats,
                           std::shared_ptr<HWResourceManager> hwResources,
                           DataType type, QString media, QFuture<uvg_rtp::media_stream *> mstream):
  Filter(id, "RTP Sender " + media, stats, hwResources, type, DT_NONE),
  type_(type),
  mstream_(nullptr),
  frame_(0),
  sessionID_(sessionID),
  rtpFlags_(RTP_NO_FLAGS)
{
  updateSettings();

  switch (type)
  {
    case DT_HEVCVIDEO:
      dataFormat_ = RTP_FORMAT_H265;
      break;

    case DT_OPUSAUDIO:
      dataFormat_ = RTP_FORMAT_OPUS;
      break;

    default:
      dataFormat_ = RTP_FORMAT_GENERIC;
      break;
  }

  connect(&watcher_, &QFutureWatcher<uvg_rtp::media_stream *>::finished,
          [this]()
          {
            if (!(mstream_ = watcher_.result()))
              emit zrtpFailure(sessionID_);
          });

  watcher_.setFuture(mstream);
}

UvgRTPSender::~UvgRTPSender()
{
}

void UvgRTPSender::updateSettings()
{
  // called in case we later decide to add some settings to filter
  Filter::updateSettings();

  if (type_ == DT_HEVCVIDEO)
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

      // discrete framer doesn't like start codes
      removeStartCodes_ = true;

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
