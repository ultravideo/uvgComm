#include "camerainfo.h"

#include <QCameraInfo>
#include <QDebug>


CameraInfo::CameraInfo()
{
  //dshow_initCapture();
}


QStringList CameraInfo::getVideoDevices()
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  QStringList list;

  qDebug() << "Found" << cameras.size() << "cameras";
  for (int i = 0; i < cameras.size(); ++i)
  {
    list.push_back(cameras.at(i).description());
  }

  return list;
}


void CameraInfo::getVideoFormats(int deviceID, QStringList& formats)
{
  formats.clear();

  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  QList<QVideoFrame::PixelFormat> p_formats = camera->supportedViewfinderPixelFormats();
  qDebug() << "Found" << p_formats.size() <<  "formats for deviceID:" << deviceID;
}

void CameraInfo::getFormatResolutions(int deviceID, QString format, QStringList &resolutions)
{
  resolutions.clear();

  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  QList<QSize> supporteResolutions = camera->supportedViewfinderResolutions();
  qDebug() << "Found" << supporteResolutions.size() <<  "resolutions for deviceID:" << deviceID;

  for (int i = 0; i < supporteResolutions.size(); ++i)
  {
    resolutions.push_back(QString::number(supporteResolutions[i].width()) + "x" +
                          QString::number(supporteResolutions[i].height()));
  }
}


void CameraInfo::getFramerates(int deviceID, QString format, int resolutionID)
{

}

void CameraInfo::getCapability(int deviceIndex, int format, uint16_t resolutionIndex,
                               QSize& resolution, double& framerate, QString& formatText)
{
  /*
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
  */
}



std::unique_ptr<QCamera> CameraInfo::loadCamera(int deviceID)
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if(deviceID == -1 || deviceID >= cameras.size())
  {
    qDebug() << "ERROR: Invalid deviceID for getVideoCapabilities";
  }

  std::unique_ptr<QCamera> camera
      = std::unique_ptr<QCamera>(new QCamera(cameras.at(deviceID)));
  camera->load();

  return camera;
}
