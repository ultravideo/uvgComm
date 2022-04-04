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
                             std::shared_ptr<ResourceAllocator> hwResources,
                             QList<VideoInterface *> widgets, uint32_t sessionID):
  Filter(id, "Display", stats, hwResources, DT_RGB32VIDEO, DT_NONE),
  horizontalMirroring_(false),
  widgets_(widgets),
  sessionID_(sessionID)
{

  if (widgets.empty())
  {
    Logger::getLogger()->printProgramError(this, "No widgest were provided");
    return;
  }

  if (widgets.at(0) != nullptr)
  {
    VideoFormat chosenFormat = widgets.at(0)->supportedFormat();

    if (chosenFormat == VIDEO_RGB32)
    {
      input_ = DT_RGB32VIDEO;
    }
    else if (chosenFormat == VIDEO_YUV420)
    {
      input_ = DT_YUV420VIDEO;
    }

    for (auto& widget : widgets)
    {
      if (widget->supportedFormat() != chosenFormat)
      {
        Logger::getLogger()->printProgramWarning(this, "The input formats of display widgets differ");
      }
    }

    widgets.at(0)->setStats(stats);
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
    case DT_RGB32VIDEO:
      format = QImage::Format_RGB32;
      break;
    case DT_YUV420VIDEO:
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
      input = normalizeOrientation(std::move(input), horizontalMirroring_);

      QImage image(
            input->data.get(),
            input->vInfo->width,
            input->vInfo->height,
            format);

      int32_t delay = QDateTime::currentMSecsSinceEpoch() - input->presentationTime;

      for (auto& widget : widgets_)
      {
        widget->inputImage(std::move(input->data), image, input->presentationTime);
      }

      if( sessionID_ != 1111)
        getStats()->receiveDelay(sessionID_, "Video", delay);
    }
    input = getInput();
  }
}
