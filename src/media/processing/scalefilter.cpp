#include "scalefilter.h"

#include "common.h"

#include <QImage>
#include <QDebug>

ScaleFilter::ScaleFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Scaler", stats, RGB32VIDEO, RGB32VIDEO),
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
    printDebug(DEBUG_PROGRAM_ERROR, this, "Size not set for scaler.");
    return;
  }

  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    if(input->height == 0 || input->width == 0 || input->data_size == 0)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "The resolution of input image for scaler is not set.",
                {"Width", "Height"}, {QString::number(input->width), QString::number(input->height)});
      return;
    }

    QImage::Format format;

    switch(input->type)
    {
      case RGB32VIDEO:
      {
        format = QImage::Format_RGB32;
        break;
      }
      default:
      {
        printDebug(DEBUG_PROGRAM_ERROR, this,  "Wrong video format for scaler.",
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
        input->width,
        input->height,
        QImage::Format_RGB32);

  QImage scaled = image.scaled(newSize_);
  if(newSize_.width() * newSize_.height()
     > input->width * input->height)
  {
    input->data = std::unique_ptr<uchar[]>(new uchar[scaled.sizeInBytes()]);
  }
  memcpy(input->data.get(), scaled.bits(), scaled.sizeInBytes());
  input->width = newSize_.width();
  input->height = newSize_.height();
  input->data_size = scaled.sizeInBytes();
  return input;
}
