#include "displayfilter.h"

#include "ui/gui/videointerface.h"
#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

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
      for (int i = widgets_.size() - 1; i > -1; --i)
      {
        if (widgets_.at(i)->isVisible())
        {

          /* This is a bit of a hack in that multiple widgets are only used for
         * the self view. The first index contains the self view (if this display filter
         * is used for selfviews and not peer views) and needs the horizontal mirroring
         * whereas other don't want it. We also must copy the data for all but the
         * last one since the life of the the data in widgets differs.
         *
         * TODO: This copying could probably be eliminated by using shared_ptr in widgets */

          input = deliverFrame(widgets_.at(i), std::move(input),
                               format, i != 0,
                               horizontalMirroring_ && i == 0);
        }
      }

      int32_t delay = QDateTime::currentMSecsSinceEpoch() - input->presentationTime;

      if( sessionID_ != 1111)
        getStats()->receiveDelay(sessionID_, "Video", delay);
    }
    input = getInput();
  }
}


std::unique_ptr<Data> DisplayFilter::deliverFrame(VideoInterface* screen,
                                                  std::unique_ptr<Data> input,
                                                  QImage::Format format,
                                                  bool useCopy, bool mirrorHorizontally)
{
  std::unique_ptr<uchar[]> tmpData = nullptr;

  // replace input data with copy
  if (useCopy)
  {
    // save input data temporarily
    tmpData = std::move(input->data);

    // create a copy to input data structure for normalize orientation
    input->data = std::unique_ptr<uchar[]>(new uchar[input->data_size]);
    memcpy(input->data.get(), tmpData.get(), input->data_size);
  }

  bool verticalOrientation = input->vInfo->flippedVertically;
  bool horizontalOrientation = input->vInfo->flippedHorizontally;
  input = normalizeOrientation(std::move(input), mirrorHorizontally);

  QImage image(
        input->data.get(),
        input->vInfo->width,
        input->vInfo->height,
        format);

  screen->inputImage(std::move(input->data), image, input->presentationTime);

  if (useCopy)
  {
    input->vInfo->flippedVertically = verticalOrientation;
    input->vInfo->flippedHorizontally = horizontalOrientation;
    input->data = std::move(tmpData);
  }

  return input;
}
