#include "scalefilter.h"

#include "common.h"
#include "logger.h"

#include <QImage>

ScaleFilter::ScaleFilter(QString id, StatisticsInterface *stats,
                         std::shared_ptr<HWResourceManager> hwResources):
  Filter(id, "Scaler", stats, hwResources, DT_RGB32VIDEO, DT_RGB32VIDEO),
  newSize_(QSize(0,0))
{

}

void ScaleFilter::setResolution(QSize newResolution)
{
  newSize_ = newResolution;
}

void ScaleFilter::process()
{
  if(newSize_ == QSize(0,0))
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Size not set for scaler.");
    return;
  }

  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    if(input->vInfo->height == 0 || input->vInfo->width == 0 || input->data_size == 0)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
                                      "The resolution of input image for scaler is not set.",
                                      {"Width", "Height"}, {QString::number(input->vInfo->width),
                                       QString::number(input->vInfo->height)});
      return;
    }

    QImage::Format format;

    switch(input->type)
    {
      case DT_RGB32VIDEO:
      {
        format = QImage::Format_RGB32;
        break;
      }
      default:
      {
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,  "Wrong video format for scaler.",
                                        {"Input type"},{QString::number(input->type)});
        format = QImage::Format_Invalid;
        break;
      }
    }

    if(format != QImage::Format_Invalid)
    {
      input = scaleFrame(std::move(input));
    }
    sendOutput(std::move(input));
  }
}

std::unique_ptr<Data> ScaleFilter::scaleFrame(std::unique_ptr<Data> input)
{
  QImage image(
        input->data.get(),
        input->vInfo->width,
        input->vInfo->height,
        QImage::Format_RGB32);

  QImage scaled = image.scaled(newSize_);
  if(newSize_.width() * newSize_.height()
     > input->vInfo->width * input->vInfo->height)
  {
    input->data = std::unique_ptr<uchar[]>(new uchar[scaled.sizeInBytes()]);
  }
  memcpy(input->data.get(), scaled.bits(), scaled.sizeInBytes());
  input->vInfo->width = newSize_.width();
  input->vInfo->height = newSize_.height();
  input->data_size = scaled.sizeInBytes();
  return input;
}
