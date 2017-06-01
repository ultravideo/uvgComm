#include "dshowcamerafilter.h"
#include "statisticsinterface.h"

#include "dshow/capture_interface.h"

#include <QSettings>
#include <QDateTime>
#include <QDebug>

DShowCameraFilter::DShowCameraFilter(QString id, StatisticsInterface *stats)
  :Filter(id, "Camera", stats, false, true),
    capabilityID_(0)
{}

void DShowCameraFilter::init()
{
  dshow_initCapture();
  int8_t count;
  dshow_queryDevices(&devices, &count);

  QSettings settings;

  QString deviceName = settings.value("video/Device").toString();
  int deviceID = settings.value("video/DeviceID").toInt();

  qDebug() << "Camera Device ID:" << deviceID << "Name:" << deviceName;

  if(count == 0)
    return;

  if(deviceID == -1)
    deviceID = 0;

  if(deviceID < count)
  {
    // if the deviceID has changed
    if(devices[deviceID] != deviceName)
    {
      // search for device with same name
      for(int i = 0; i < count; ++i)
      {
        if(devices[i] == deviceName)
        {
          deviceID = i;
          break;
        }
      }
      // previous camera could not be found, use default.
      deviceID = 0;
    }
  }

  if (!dshow_selectDevice(deviceID))
  {
    qDebug() << "Could not select device from settings. Trying first";
    if(!dshow_selectDevice(0))
    {
      qDebug() << "Error selecting device!";
      return;
    }
  }

  capabilityID_ = settings.value("video/ResolutionID").toInt();

  if(capabilityID_ == -1)
  {
    qWarning() << "WARNING: Settings Id was -1";
    capabilityID_ = 0;
  }

  qDebug() << "Camera capability ID:" << capabilityID_;

  dshow_getDeviceCapabilities(&list_, &count);
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
  dshow_play();

  uint8_t *data;
  uint32_t size;
  while (dshow_queryFrame(&data, &size));

  qDebug() << "Start taking frames from DShow camera";

  run_ = true;

  while (run_) {
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

      newImage->height = list_[capabilityID_].height;
      newImage->width = list_[capabilityID_].width;

      newImage->source = LOCAL;
      newImage->framerate = list_[capabilityID_].fps;

      //qDebug() << "Frame generated. Width: " << newImage->width
      //         << ", height: " << newImage->height << "Framerate:" << newImage->framerate;

      std::unique_ptr<Data> u_newImage( newImage );
      sendOutput(std::move(u_newImage));
    }
  }

  // TODO: Either empty dshow buffer or fix the dshow stop should also make it possible to restart
  dshow_stop();
}
