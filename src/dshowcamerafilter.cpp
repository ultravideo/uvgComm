#include "dshowcamerafilter.h"
#include "statisticsinterface.h"

#include "dshow/capture_interface.h"

#include <QSettings>
#include <QDateTime>
#include <QDebug>

DShowCameraFilter::DShowCameraFilter(QString id, StatisticsInterface *stats)
  :Filter(id, "Camera", stats, false, true),
    deviceID_(0),
    capabilityID_(0),
    exited_(true)
{}

DShowCameraFilter::~DShowCameraFilter()
{}

void DShowCameraFilter::updateSettings()
{
  stop();
  while(!exited_)
    _sleep(1);
  init();
  start();
}

void DShowCameraFilter::init()
{
  dshow_initCapture();
  int8_t count;
  char **devices;
  dshow_queryDevices(&devices, &count);

  QSettings settings;

  QString deviceName = settings.value("video/Device").toString();
  deviceID_ = settings.value("video/DeviceID").toInt();

  qDebug() << "Camera Device ID:" << deviceID_ << "Name:" << deviceName;

  if(count == 0)
  {
    deviceID_ = -1;
    capabilityID_ = -1;
    return;
  }

  if(deviceID_ == -1)
    deviceID_ = 0;

  if(deviceID_ < count)
  {
    // if the deviceID has changed
    if(devices[deviceID_] != deviceName)
    {
      // search for device with same name
      for(int i = 0; i < count; ++i)
      {
        if(devices[i] == deviceName)
        {
          deviceID_ = i;
          break;
        }
      }
      // previous camera could not be found, use default.
      deviceID_ = 0;
    }
  }

  if (!dshow_selectDevice(deviceID_))
  {
    qDebug() << "Could not select device from settings. Trying first";
    if(!dshow_selectDevice(0))
    {
      qDebug() << "Error selecting device!";
      return;
    }
  }

  capabilityID_ = settings.value("video/ResolutionID").toInt();
  //capabilityID_ = 0;

  if(capabilityID_ == -1)
  {
    qWarning() << "WARNING: Settings Id was -1";
    capabilityID_ = 0;
  }

  qDebug() << "Camera capability ID:" << capabilityID_;

  dshow_getDeviceCapabilities(&list_, &count);

  if(capabilityID_ > count)
  {
    qWarning() << "WARNING: Device capability exceeds the maximum number. Maybe the device has changed "
                  "and capabilities was not updated?";
    capabilityID_ = 0;
  }
  if (!dshow_setDeviceCapability(capabilityID_))
  {
    qDebug() << "Error selecting capability!";
    return;
  }

  stats_->videoInfo(list_[capabilityID_].fps, QSize(list_[capabilityID_].width, list_[capabilityID_].height));

  qDebug() << "Starting DShow camera";
  dshow_startCapture();
}

void DShowCameraFilter::stop()
{
  run_ = false;
}

void DShowCameraFilter::run()
{
  Q_ASSERT(list_ != 0);
  exited_ = false;
  dshow_play();

  uint8_t *data;
  uint32_t size;
  while (dshow_queryFrame(&data, &size))
  {
    delete data;
  }

  qDebug() << "Start taking frames from DShow camera";

  run_ = true;

  while (run_ && capabilityID_ != -1 && deviceID_ != -1) {
    // sleep half of what is between frames. TODO sleep correct amount
    _sleep(500/list_[capabilityID_].fps);

    while (dshow_queryFrame(&data, &size))
    {
      Data * newImage = new Data;

      // set time
      timeval present_time;
      present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
      present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;

      newImage->presentationTime = present_time;
      newImage->type = RGB32VIDEO;
      newImage->data = std::unique_ptr<uchar[]>(data);
      data = 0;
      newImage->data_size = size;

      // take camera height and width and make sure they are divisable by 8 for kvazaar
      newImage->height = list_[capabilityID_].height - list_[capabilityID_].height%8;
      newImage->width = list_[capabilityID_].width - list_[capabilityID_].width%8;

      newImage->source = LOCAL;
      newImage->framerate = list_[capabilityID_].fps;

      //qDebug() << "Frame generated. Width: " << newImage->width
      //         << ", height: " << newImage->height << "Framerate:" << newImage->framerate;

      std::unique_ptr<Data> u_newImage( newImage );
      sendOutput(std::move(u_newImage));
    }
  }

  qDebug() << "Stop taking frames from DShow";

  // TODO: Either empty dshow buffer or fix the dshow stop should also make it possible to restart
  dshow_stop();

  exited_ = true;
}
