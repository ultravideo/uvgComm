#include "displayfilter.h"

#include <QImage>
#include <QtDebug>

#include "videowidget.h"


DisplayFilter::DisplayFilter(VideoWidget *widget):widget_(widget)
{
  name_ = "DispF";
  widget_->show();
}

void DisplayFilter::process()
{
  qDebug() << "DispF: Drawing input";
  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    //Q_ASSERT(input->type == RPG32VIDEO);

    qDebug() << "Sending image for drawing with size: " << input->data_size
             << "with width: " << input->width << ", height: " << input->height;
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

      //image = image.mirrored();
      widget_->inputImage(std::move(input->data), image);
    }
    input = getInput();
  }

}
