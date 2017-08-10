#include "displayfilter.h"

#include "videowidget.h"

#include <QImage>
#include <QtDebug>
#include <QDateTime>

#include "statisticsinterface.h"

DisplayFilter::DisplayFilter(QString id, StatisticsInterface *stats,
                             VideoWidget *widget, uint32_t peer):
  Filter(id, "Display", stats, RGB32VIDEO, NONE),
  widget_(widget),
  peer_(peer)
{
  mirrored_ = false;
  widget_->show();
}

DisplayFilter::~DisplayFilter()
{}

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
      qCritical() << "DispF: Wrong type of display input:" << input->type;
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

      image = image.mirrored(mirrored_, mirrored_);

      int32_t delay = QDateTime::currentMSecsSinceEpoch() -
          (input->presentationTime.tv_sec * 1000 + input->presentationTime.tv_usec/1000);

      if( peer_ != 1111)
        stats_->receiveDelay(peer_, "Video", delay);

      widget_->inputImage(std::move(input->data), image);
    }
    input = getInput();
  }

}
