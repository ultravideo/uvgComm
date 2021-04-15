#include "screensharefilter.h"

#include <QWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QDateTime>

#include "common.h"
#include "settingskeys.h"

ScreenShareFilter::ScreenShareFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Screen Sharing", stats, NONE, RGB32VIDEO),
screenID_(0)
{}


bool ScreenShareFilter::init()
{
  updateSettings();

  return Filter::init();
}


void ScreenShareFilter::updateSettings()
{
  bool enabled = settingEnabled(SettingsKey::screenShareStatus);

  if (enabled)
  {
    printNormal(this, "Enabling screen share filter");

    screenID_ = settingValue(SettingsKey::userScreenID);

    if (screenID_ >= QGuiApplication::screens().size())
    {
      screenID_ = 0;
    }

    currentFramerate_ = settingValue(SettingsKey::videoFramerate);

    currentResolution_.setWidth(settingValue(SettingsKey::videoResultionWidth));
    currentResolution_.setHeight(settingValue(SettingsKey::videoResultionHeight));

    sendTimer_.setSingleShot(false);
    sendTimer_.setInterval(1000/currentFramerate_);
    connect(&sendTimer_, &QTimer::timeout, this, &ScreenShareFilter::sendScreen);
    sendTimer_.start();

    // take first screenshot immediately so we don't have to wait
    sendScreen();
  }
  else
  {
    printNormal(this, "Disabling screen share filter");
    sendTimer_.stop();
  }
}


void ScreenShareFilter::process()
{
  QScreen *screen = QGuiApplication::primaryScreen();
  if (screenID_ < QGuiApplication::screens().size())
  {
    screen = QGuiApplication::screens().at(screenID_);
  }

  if (!screen)
  {
    printError(this, "Couldn't get screen");
    return;
  }

  if (currentResolution_.width() != screen->size().width() - screen->size().width()%8 ||
      currentResolution_.height() != screen->size().height() - screen->size().height()%8)
  {
    QString currentResolution = QString::number(currentResolution_.width()) + "x" +
        QString::number(currentResolution_.height());

    QString screenResolution = QString::number(screen->size().width()) + "x" +
        QString::number(screen->size().height());

    printDebug(DEBUG_PROGRAM_ERROR, this, "Current resolution differs from screen size",
               {"Current", "Screen resolution"}, {currentResolution, screenResolution});
    return;
  }

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
  newImage->width = currentResolution_.width();
  newImage->height = currentResolution_.height();
  newImage->source = LOCAL;
  newImage->framerate = currentFramerate_;

  std::unique_ptr<Data> u_newImage( newImage );

  Q_ASSERT(u_newImage->data);
  sendOutput(std::move(u_newImage));
}


void ScreenShareFilter::sendScreen()
{
  wakeUp();
}
