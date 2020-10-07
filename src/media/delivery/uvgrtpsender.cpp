#include <QSettings>

#include "uvgrtpsender.h"
#include "statisticsinterface.h"
#include "common.h"

UvgRTPSender::UvgRTPSender(QString id, StatisticsInterface *stats,
                           DataType type, QString media, uvg_rtp::media_stream *mstream):
  Filter(id, "RTP Sender " + media, stats, type, NONE),
  type_(type),
  mstream_(mstream),
  frame_(0)
{
  updateSettings();

  switch (type)
  {
    case HEVCVIDEO:
      dataFormat_ = RTP_FORMAT_H265;
      break;

    case OPUSAUDIO:
      dataFormat_ = RTP_FORMAT_OPUS;
      break;

    default:
      dataFormat_ = RTP_FORMAT_GENERIC;
      break;
  }
}

UvgRTPSender::~UvgRTPSender()
{
}

void UvgRTPSender::updateSettings()
{
  // called in case we later decide to add some settings to filter
  Filter::updateSettings();

  if (type_ == HEVCVIDEO)
  {
    QSettings settings("kvazzup.ini", QSettings::IniFormat);

    uint32_t vps   = settings.value("video/VPS").toInt();
    uint16_t intra = settings.value("video/Intra").toInt();

    if (settings.value("video/Slices").toInt() == 1)
    {
      // 4k 30 fps uses can fit to max 200 slices
      int maxSlices = 200;

      // this buffersize is intended to hold at least one intra frame so we can
      // delete all picture that do not belong to it. TODO: not implemented
      maxBufferSize_    = maxSlices * vps * intra;
      removeStartCodes_ = false;
    }
    else
    {
      // the number of frames between parameter sets
      maxBufferSize_ = vps * intra;

      // discrete framer doesn't like start codes
      removeStartCodes_ = true;
    }

    printDebug(DEBUG_NORMAL, this,  "Updated buffersize", {"Size"}, {QString::number(maxBufferSize_)});
  }
}


void UvgRTPSender::process()
{
  rtp_error_t ret;
  std::unique_ptr<Data> input = getInput();

  while (input)
  {
    ret = mstream_->push_frame(std::move(input->data), input->data_size, RTP_NO_FLAGS);

    if (ret != RTP_OK)
    {
      printDebug(DEBUG_ERROR, this,  "Failed to send data", { "Error" }, { QString(ret) });
    }

    getStats()->addSendPacket(input->data_size);

    input = getInput();
  }
}
