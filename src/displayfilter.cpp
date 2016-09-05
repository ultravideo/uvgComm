#include "displayfilter.h"

#include "videowidget.h"

#include <QImage>
#include <QtDebug>

DisplayFilter::DisplayFilter(StatisticsInterface *stats, VideoWidget *widget):
  Filter("Display", stats),
  widget_(widget)
{
  mirrored_ = false;
  widget_->show();
}

void DisplayFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {

    QImage::Format format;

    switch(input->type)
    {
    case RGB32VIDEO:
      format = QImage::Format_RGB32;
      break;
    default:
      qCritical() << "DispF: Wrong type of display input.";
       format = QImage::Format_Invalid;
      break;
    }

    if(format != QImage::Format_Invalid)
    {
      QImage image(
            input->data.get(),
            input->width,
            input->height,
            format);

      image = image.mirrored(true, mirrored_);

      widget_->inputImage(std::move(input->data), image);
    }
    input = getInput();
  }

}
