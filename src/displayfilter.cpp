#include "displayfilter.h"

#include <QImage>
#include <QtDebug>

#include "videowidget.h"


DisplayFilter::DisplayFilter(VideoWidget *widget):widget_(widget)
{
  widget_->show();
}

void DisplayFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    Q_ASSERT(input->type == 0);

    qDebug() << "Sending image for drawing with size: " << input->data_size
             << "with width: " << input->width << ", height: " << input->height;
    QImage image(
          input->data.get(),
          input->width,
          input->height,
          QImage::Format_RGB32);

    image = image.mirrored();
    widget_->inputImage(std::move(input->data), image);

    input = getInput();
  }

}
