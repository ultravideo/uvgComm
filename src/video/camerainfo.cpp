#include "camerainfo.h"

#include "video/dshow/capture_interface.h"

#include <QDebug>

CameraInfo::CameraInfo()
{
  dshow_initCapture();
}

QStringList CameraInfo::getVideoDevices()
{
  char** devices;
  int8_t count = 0;
  dshow_queryDevices(&devices, &count);

  QStringList list;

  qDebug() << "Found " << (int)count << " devices: ";
  for(int i = 0; i < count; ++i)
  {
    qDebug() << "[" << i << "] " << devices[i];
    list.push_back(devices[i]);
  }
  return list;
}

void CameraInfo::getVideoCapabilities(int deviceID, QStringList& formats, QList<QStringList>& resolutions)
{
  formats.clear();
  resolutions.clear();
  if (dshow_selectDevice(deviceID) || dshow_selectDevice(0))
  {
    int8_t count = 0;
    deviceCapability *capList;
    dshow_getDeviceCapabilities(&capList, &count);

    qDebug() << "Found" << (int)count << "capabilities";
    for(int i = 0; i < count; ++i)
    {
      if(formats.empty() || formats.back() != QString(capList[i].format))
      {
        formats.push_back(QString(capList[i].format));
        resolutions.push_back(QStringList());
      }
      resolutions.back().push_back(QString::number(capList[i].width) + "x" +
                     QString::number(capList[i].height) + " " +
                     QString::number(capList[i].fps) + " fps");
    }
  }
  return;
}

void CameraInfo::getCapability(int deviceIndex,
                             int capabilityIndex,
                             QSize& resolution,
                             double& framerate,
                             QString& format)
{
  char **devices;
  int8_t count;
  dshow_queryDevices(&devices, &count); // this has to be done because of the api

  if (dshow_selectDevice(deviceIndex) || dshow_selectDevice(0))
  {
    int8_t count;
    deviceCapability *capList;
    dshow_getDeviceCapabilities(&capList, &count);

    if(count == 0)
    {
      qDebug() << "No capabilites found";
      return;
    }
    if(count < capabilityIndex)
    {
      capabilityIndex = 0;
      qDebug() << "Capability id not found";
    }

    resolution = QSize(capList[capabilityIndex].width,capList[capabilityIndex].height);
    framerate = capList[capabilityIndex].fps;
    format = capList[capabilityIndex].format;
  }
  else
  {
    qWarning() << "WARNING: Failed to select device for capacity information.";
  }
}
