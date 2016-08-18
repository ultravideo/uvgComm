#include "displayfilter.h"

#include <QImage>
#include <QtDebug>

#include "videowidget.h"


DisplayFilter::DisplayFilter(VideoWidget *widget):widget_(widget)
{
  name_ = "DispF";
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

      if(mirrored_)
        image = image.mirrored(true, true);
      else
        image = image.mirrored(true, false);
      if(!scale_.isEmpty() && scale_.isValid())
        image = image.scaled(scale_);
      else
      {
        image = image.scaled(widget_->size(), Qt::KeepAspectRatio);
      }

      QSize padding = (widget_->size() - image.size())/2;

      widget_->inputImage(std::move(input->data), image, padding);
    }
    input = getInput();
  }

}
