#include "dshowcamerafilter.h"

#include "statisticsinterface.h"

#include <QDateTime>
#include <QDebug>

DShowCameraFilter::DShowCameraFilter(QString id, StatisticsInterface *stats)
  :Filter(id, "Camera", stats, false, true),
    deviceID_(0),
    settingsID_(33)
{}

void DShowCameraFilter::init()
{
  dshow_initCapture();
  int8_t count;
  dshow_queryDevices(&devices, &count);
  if (!dshow_selectDevice(deviceID_))
  {
    qDebug() << "Error selecting device!";
    return;
  }

  qDebug() << "Found " << (int)count << " devices: ";
  for (int i = 0; i < count; i++) {
    qDebug() << "[" << i << "] " << devices[i];
  }

  dshow_getDeviceCapabilities(&list_, &count);
  if (!dshow_setDeviceCapability(settingsID_))
  {
    qDebug() << "Error selecting capability!";
    return;
  }

  qDebug() << "Found " << (int)count << " capabilities: ";
  for (int i = 0; i < count; i++) {
    qDebug()  << "[" << i << "] " << list_[i].width << "x" << list_[i].height  << " " << list_[i].fps << "fps";
  }

  stats_->videoInfo(list_[settingsID_].fps, QSize(list_[settingsID_].width, list_[settingsID_].height));

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

  qDebug() << "Start taking frames from DShow camera";

  run_ = true;

  while (run_) {
    _sleep(1);
    uint8_t *data;
    uint32_t size;
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

      newImage->height = list_[settingsID_].height;
      newImage->width = list_[settingsID_].width;

      newImage->source = LOCAL;
      newImage->framerate = list_[settingsID_].fps;

      //qDebug() << "Frame generated. Width: " << newImage->width
      //         << ", height: " << newImage->height << "Framerate:" << newImage->framerate;

      std::unique_ptr<Data> u_newImage( newImage );
      sendOutput(std::move(u_newImage));
    }
  }
}
