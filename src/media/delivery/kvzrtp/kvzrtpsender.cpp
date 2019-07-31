#include <QSettings>
#include <QDebug>

#include "kvzrtpsender.h"
#include "statisticsinterface.h"
#include "common.h"

KvzRTPSender::KvzRTPSender(QString id, StatisticsInterface *stats,
                                       DataType type, QString media, kvz_rtp::writer *writer):
  Filter(id, "Framed_Source_" + media, stats, type, NONE),
  type_(type),
  stop_(false),
  writer_(writer),
  frame_(0)
{
  updateSettings();

  switch (type)
  {
    case HEVCVIDEO:
      dataFormat_ = RTP_FORMAT_HEVC;
      break;

    case OPUSAUDIO:
      dataFormat_ = RTP_FORMAT_OPUS;
      break;

    default:
      dataFormat_ = RTP_FORMAT_GENERIC;
      break;
  }
}

KvzRTPSender::~KvzRTPSender()
{
}

void KvzRTPSender::updateSettings()
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

    printDebug(DEBUG_NORMAL, this, DC_PROCESS_MEDIA, "Updated buffersize", {"Size"}, {QString::number(maxBufferSize_)});
  }
}

void KvzRTPSender::start()
{
  Filter::start();
}

void KvzRTPSender::stop()
{
  Filter::stop();
  stop_ = true;
}

void KvzRTPSender::process()
{
  rtp_error_t ret;
  std::unique_ptr<Data> currentFrame;

  while (!stop_)
  {
    while ((currentFrame = getInput()))
    {
      ret = writer_->push_frame(currentFrame->data.get(),
                                currentFrame->data_size,
                                dataFormat_, (90000 / 24) * ++frame_);

      if (ret != RTP_OK)
      {
        printDebug(DEBUG_ERROR, this, DC_PROCESS_MEDIA, "Failed to send data", { "Error" }, { QString(ret) });
        break;
      }

      getStats()->addSendPacket(currentFrame->data_size);
    }
    return;
  }
}
