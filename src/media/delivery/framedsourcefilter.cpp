#include <QSettings>
#include <QDebug>

#include "framedsourcefilter.h"
#include "statisticsinterface.h"
#include "common.h"

#include "../rtplib/src/util.hh"

FramedSourceFilter::FramedSourceFilter(QString id, StatisticsInterface *stats,
                                       DataType type, QString media, RTPWriter *writer):
  Filter(id, "Framed_Source_" + media, stats, type, NONE),
  type_(type),
  stop_(false),
  separateInput_(false), // TODO mikä järki tässä on???
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

FramedSourceFilter::~FramedSourceFilter()
{
}

void FramedSourceFilter::updateSettings()
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

     qDebug() << "Settings," << getName() << ": updated buffersize to" << maxBufferSize_;

     if (settings.value("video/liveCopying").isValid())
     {
       separateInput_ = (settings.value("video/liveCopying").toInt() == 1);
     }
     else
     {
       qDebug() << "ERROR: Missing settings value flip threads";
     }
  }
}

void FramedSourceFilter::start()
{
  qDebug() << "\n\n\nDUMMY FRAMEDSOURCEFILTER::START\n\n\n";

  if (separateInput_)
  {
    /* TODO:  */
    /* afterEvent_ = envir().taskScheduler().createEventTrigger((TaskFunc*)FramedSource::afterGetting); */

    qDebug() << "Iniating, FramedSource : Creating trigger for framedSource:";
             //<< getName() << "Trigger ID: " << afterEvent_;
  }

  Filter::start();
}

void FramedSourceFilter::stop()
{
  qDebug() << "\n\n\nDUMMY FRAMEDSOURCEFILTER::STOP\n\n\n";
  /* TODO:  */
}

void FramedSourceFilter::process()
{
  /* qDebug() << "\n\n\nDUMMY FRAMEDSOURCEFILTER::PROCESS\n\n\n"; */

  while (true)
  {
    /* framePointerReady_.acquire(1); */

    if (stop_)
    {
      return;
    }

    std::unique_ptr<Data> currentFrame = getInput();

    // TODO mitä ihmettä tässä tapahtuu?
    if (currentFrame == nullptr)
    {
      /* fFrameSize = 0; */
      /* sendFrame(); */
      qDebug() << "hmmmm...";
      return;
    }

    while (currentFrame)
    {
      /* qDebug() << "do stuff"; */
      /* copyFrameToBuffer(std::move(currentFrame)); */
      /* sendFrame(); */

      frame_++;
      if (writer_->pushFrame(currentFrame->data.get(), currentFrame->data_size, dataFormat_, (90000 / 24) * frame_) < 0)
      {
        qDebug() << "failed to send data!";
        break;
      }

      getStats()->addSendPacket(currentFrame->data_size);
      currentFrame = nullptr;

      // TODO mitä ihmeen säätöä tämä loppu on?

      /* framePointerReady_.acquire(1); */
      if (stop_)
      {
        return;
      }

      // TODO mitä ihmettä tässä tapahtuu?
      currentFrame = getInput();

      // copy additional NAL units, if available.
      if(currentFrame == nullptr)
      {
        /* fFrameSize = 0; */
        /* sendFrame(); */
        return;
      }
    }
  }

}
