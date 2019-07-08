#include "displayfilter.h"

#include "ui/gui/videointerface.h"
#include "statisticsinterface.h"

#include "common.h"

#include <QImage>
#include <QtDebug>
#include <QDateTime>
#include <QSettings>

DisplayFilter::DisplayFilter(QString id, StatisticsInterface *stats,
                             VideoInterface *widget, uint32_t peer):
  Filter(id, "Display", stats, RGB32VIDEO, NONE),
  horizontalMirroring_(false),
  verticalMirroring_(false),
  widget_(widget),
  peer_(peer)
{
  if(widget->supportedFormat() == VIDEO_RGB32)
  {
    input_ = RGB32VIDEO;
  }
  else if(widget->supportedFormat() == VIDEO_YUV420)
  {
    input_ = YUV420VIDEO;
  }
  widget_->setStats(stats);
  updateSettings();
}

DisplayFilter::~DisplayFilter()
{}

void DisplayFilter::updateSettings()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  if(settings.value("video/flipViews").isValid())
  {
    flipEnabled_ = (settings.value("video/flipViews").toInt() == 1);
  }
  else
  {
    printDebug(DEBUG_ERROR, "CameraInfo", DC_SETTINGS,
               "Missing settings value flip threads.");
  }

  Filter::updateSettings();
}


void DisplayFilter::process()
{
  // TODO: The display should try to mimic the framerate if possible,
  // without adding too much latency

  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    QImage::Format format;

    switch(input->type)
    {
    case RGB32VIDEO:
      format = QImage::Format_RGB32;
      break;
    case YUV420VIDEO:
      format = QImage::Format_Invalid;
      break;
    default:
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_PROCESS_MEDIA,
                 "Wrong type of display input.", {"Type"}, {QString::number(input->type)});
      format = QImage::Format_Invalid;
      break;
    }

    if(input->type == input_)
    {
      QImage image(
            input->data.get(),
            input->width,
            input->height,
            format);

      if(flipEnabled_)
      {
        image = image.mirrored(horizontalMirroring_, verticalMirroring_);
      }

      int32_t delay = QDateTime::currentMSecsSinceEpoch() -
          (input->presentationTime.tv_sec * 1000 + input->presentationTime.tv_usec/1000);

      widget_->inputImage(std::move(input->data),image);

      if( peer_ != 1111)
        getStats()->receiveDelay(peer_, "Video", delay);
    }
    input = getInput();
  }
}
