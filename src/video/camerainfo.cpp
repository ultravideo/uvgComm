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

void CameraInfo::getCapability(int deviceIndex, uint16_t format, uint16_t resolutionIndex,
                               QSize& resolution, double& framerate, QString& formatText)
{
  int capabilityIndex = formatToCapability(deviceIndex, format, resolutionIndex);
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
    formatText = QString(capList[capabilityIndex].format);
  }
  else
  {
    qWarning() << "WARNING: Failed to select device for capacity information.";
  }
}

// used by dshow camera to determine the index of capability
int CameraInfo::formatToCapability(uint16_t device, uint16_t formatIndex, uint16_t resolutionIndex)
{
  QStringList formats;
  QList<QStringList> resolutions;
  getVideoCapabilities(device, formats, resolutions);


  if(resolutionIndex == -1)
  {
    qDebug() << "No current index set for resolution. Using first";
    resolutionIndex = 0;
  }


  if(formatIndex == -1)
  {
    qDebug() << "No current index set for format. Using first";
    formatIndex = 0;
  }

  int currentIndex = 0;

  // add all other resolutions to form currentindex
  for(unsigned int i = 0; i <= formatIndex; ++i)
  {
    currentIndex += resolutions.at(i).size();
  }

  currentIndex += resolutionIndex;

  return currentIndex;
}
