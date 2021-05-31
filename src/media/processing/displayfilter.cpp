#include "displayfilter.h"

#include "ui/gui/videointerface.h"
#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QImage>
#include <QDateTime>
#include <QSettings>

DisplayFilter::DisplayFilter(QString id, StatisticsInterface *stats,
                             VideoInterface *widget, uint32_t sessionID):
  Filter(id, "Display", stats, RGB32VIDEO, NONE),
  horizontalMirroring_(false),
  verticalMirroring_(false),
  widget_(widget),
  sessionID_(sessionID)
{
  if (widget != nullptr)
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
  }
  else {
    Q_ASSERT(false);
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "Display Filter",
                                    "Gived nonexistant widget");
  }

  updateSettings();
}


DisplayFilter::~DisplayFilter()
{}


void DisplayFilter::updateSettings()
{
  flipEnabled_ = settingEnabled(SettingsKey::videoFlipEnabled);

  Filter::updateSettings();
}


void DisplayFilter::process()
{
  // TODO: The display should try to mimic the framerate if possible,
  // without adding too much latency

  std::unique_ptr<Data> input = getInput();
  while (input)
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
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
                                      "Wrong type of display input.", {"Type"}, 
                                      {QString::number(input->type)});
      format = QImage::Format_Invalid;
      break;
    }

    if (input->type == input_)
    {
      QImage image(
            input->data.get(),
            input->width,
            input->height,
            format);

      if (flipEnabled_)
      {
        image = image.mirrored(horizontalMirroring_, verticalMirroring_);
      }

      int32_t delay = QDateTime::currentMSecsSinceEpoch() - input->presentationTime;

      widget_->inputImage(std::move(input->data), image, input->presentationTime);

      if( sessionID_ != 1111)
        getStats()->receiveDelay(sessionID_, "Video", delay);
    }
    input = getInput();
  }
}
