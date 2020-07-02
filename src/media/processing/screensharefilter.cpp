#include "screensharefilter.h"

#include <QWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QDateTime>

const int FRAMERATE = 5;

ScreenShareFilter::ScreenShareFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Screen Sharing", stats, NONE, RGB32VIDEO)
{}


bool ScreenShareFilter::init()
{
  sendTimer_.setSingleShot(false);
  sendTimer_.setInterval(1000/FRAMERATE);
  connect(&sendTimer_, &QTimer::timeout, this, &ScreenShareFilter::sendScreen);
  sendTimer_.start();

  return Filter::init();
}

void ScreenShareFilter::process()
{
  QScreen *screen = QGuiApplication::primaryScreen();
  if (!screen)
      return;


  QPixmap screenCapture = screen->grabWindow(0);
  QImage image = screenCapture.toImage();

  // capture the frame data
  Data * newImage = new Data;

  newImage->presentationTime = QDateTime::currentMSecsSinceEpoch();
  newImage->type = output_;
  newImage->data = std::unique_ptr<uchar[]>(new uchar[image.sizeInBytes()]);

  image = image.mirrored(false, true);
  uchar *bits = image.bits();


  memcpy(newImage->data.get(), bits, image.sizeInBytes());
  newImage->data_size = image.sizeInBytes();
  // kvazaar requires divisable by 8 resolution
  newImage->width = screen->size().width() - screen->size().width()%8;
  newImage->height = screen->size().height() - screen->size().height()%8;
  newImage->source = LOCAL;
  newImage->framerate = FRAMERATE;

  std::unique_ptr<Data> u_newImage( newImage );

  Q_ASSERT(u_newImage->data);
  sendOutput(std::move(u_newImage));
}


void ScreenShareFilter::sendScreen()
{
  wakeUp();
}
