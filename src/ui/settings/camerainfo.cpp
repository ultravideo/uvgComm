#include "camerainfo.h"

#include "common.h"
#include "logger.h"

#include <QtMultimedia/QCameraInfo>

const std::map<QVideoFrame::PixelFormat, QString> pixelFormatStrings = {{QVideoFrame::Format_Invalid, "INVALID"},
                                                         {QVideoFrame::Format_ARGB32,  "ARGB32"},
                                                         {QVideoFrame::Format_ARGB32_Premultiplied, "ARGB32_Premultiplied"},
                                                         {QVideoFrame::Format_RGB32,   "RGB32"},
                                                         {QVideoFrame::Format_RGB24,   "RGB24"},
                                                         {QVideoFrame::Format_RGB565,  "RGB565"},
                                                         {QVideoFrame::Format_RGB555,  "RGB555"},
                                                         {QVideoFrame::Format_ARGB8565_Premultiplied, "ARGB8565_Premultiplied"},
                                                         {QVideoFrame::Format_BGRA32,  "BGRA32"},
                                                         {QVideoFrame::Format_BGRA32_Premultiplied, "BGRA32_Premultiplied"},
                                                         {QVideoFrame::Format_BGR32,   "BGR32"},
                                                         {QVideoFrame::Format_BGR24,   "BGR24"},
                                                         {QVideoFrame::Format_BGR565,  "BGR565"},
                                                         {QVideoFrame::Format_BGR555,  "BGR555"},
                                                         {QVideoFrame::Format_BGRA5658_Premultiplied, "BGRA5658_Premultiplied"},
                                                         {QVideoFrame::Format_AYUV444, "AYUV444"},
                                                         {QVideoFrame::Format_AYUV444_Premultiplied, "AYUV444_Premultiplied"},
                                                         {QVideoFrame::Format_YUV444,  "YUV444"},
                                                         {QVideoFrame::Format_YUV420P, "YUV420P"},
                                                         {QVideoFrame::Format_YV12,    "YV12"},
                                                         {QVideoFrame::Format_UYVY,    "UYVY"},
                                                         {QVideoFrame::Format_YUYV,    "YUYV"},
                                                         {QVideoFrame::Format_NV12,    "NV12"},
                                                         {QVideoFrame::Format_NV21, "NV21"},
                                                         {QVideoFrame::Format_IMC1, "IMC1"},
                                                         {QVideoFrame::Format_IMC2, "IMC2"},
                                                         {QVideoFrame::Format_IMC3, "IMC3"},
                                                         {QVideoFrame::Format_IMC4, "IMC4"},
                                                         {QVideoFrame::Format_Y8, "Y8"},
                                                         {QVideoFrame::Format_Y16, "Y16"},
                                                         {QVideoFrame::Format_Jpeg, "MJPG"},
                                                         {QVideoFrame::Format_CameraRaw, "CameraRaw"},
                                                         {QVideoFrame::Format_AdobeDng, "AdobeDng"},
                                                         {QVideoFrame::Format_User, "User Defined"}};

const QList<QVideoFrame::PixelFormat> kvazzupFormats = {QVideoFrame::Format_RGB32,
                                                        QVideoFrame::Format_YUV420P,
                                                        QVideoFrame::Format_Jpeg,
                                                        QVideoFrame::Format_YUYV};

CameraInfo::CameraInfo()
{}


QStringList CameraInfo::getDeviceList()
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  QStringList list;

  //Logger::getLogger()->printDebug(DEBUG_NORMAL, "Camera Info", "Get camera list",
  //          {"Cameras"}, {QString::number(cameras.size())});

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
  if (camera != nullptr)
  {
    QList<QVideoFrame::PixelFormat> p_formats = camera->supportedViewfinderPixelFormats();
    camera->unload();

    /*
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "Camera Info",
                                    "Getting text of " + QString::number(p_formats.size()) + " video formats",
                                    {"DeviceID"}, {QString::number(deviceID)}); */

    getAllowedFormats(p_formats, formats);
  }
}


void CameraInfo::getFormatResolutions(int deviceID, QString format, QStringList &resolutions)
{
  resolutions.clear();

  QCameraViewfinderSettings viewSettings;
  viewSettings.setPixelFormat(stringToPixelFormat(format));

  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  if (camera != nullptr)
  {
    QList<QSize> supportedResolutions = camera->supportedViewfinderResolutions(viewSettings);
    camera->unload();

    /*
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "Camera Info", 
                                    "Getting text of " + 
                                    QString::number(supportedResolutions.size()) + 
                                    " video resolution strings",
                                    {"DeviceID"},  {QString::number(deviceID)}); */

    for (int i = 0; i < supportedResolutions.size(); ++i)
    {
      resolutions.push_back(QString::number(supportedResolutions[i].width()) + "x" +
                            QString::number(supportedResolutions[i].height()));
    }
  }
}


void CameraInfo::getFramerates(int deviceID, QString format, int resolutionID, QStringList &ranges)
{
  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  if (camera != nullptr)
  {
    QCameraViewfinderSettings viewSettings;
    viewSettings.setPixelFormat(stringToPixelFormat(format));
    QList<QSize> resolutions  = camera->supportedViewfinderResolutions(viewSettings);

    if (0 <= resolutionID && resolutionID < resolutions.size())
    {
      viewSettings.setResolution(resolutions.at(resolutionID));
    }

    QList<QCamera::FrameRateRange> framerates = camera->supportedViewfinderFrameRateRanges(viewSettings);
    camera->unload();

    for (int i = 0; i < framerates.size(); ++i)
    {
      ranges.push_back(QString::number(framerates.at(i).maximumFrameRate));
    }
  }
}


std::unique_ptr<QCamera> CameraInfo::loadCamera(int deviceID)
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if(deviceID == -1 || deviceID >= cameras.size())
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, "CameraInfo", 
                                    "Invalid deviceID for getVideoCapabilities");
    return std::unique_ptr<QCamera> (nullptr);
  }

  std::unique_ptr<QCamera> camera
      = std::unique_ptr<QCamera>(new QCamera(cameras.at(deviceID)));
  camera->load();

  return camera;
}


QString CameraInfo::videoFormatToString(QVideoFrame::PixelFormat format)
{
  return pixelFormatStrings.at(format);
}

QVideoFrame::PixelFormat CameraInfo::stringToPixelFormat(QString format)
{
  for(auto& type : pixelFormatStrings)
  {
    if(type.second == format)
    {
      return type.first;
    }
  }

  return QVideoFrame::PixelFormat::Format_Invalid;
}


int CameraInfo::getMostMatchingDeviceID(QString deviceName, int deviceID)
{
  QStringList devices = getDeviceList();

  // no cameras have been found so we return invalid deviceID
  if (devices.empty())
  {
    return -1;
  }

  // if the deviceID is larger than is possible or if the name does not match
  if (deviceID >= devices.size() || deviceID < 0 || devices.at(deviceID) != deviceName)
  {
    // find the first camera with same name as deviceName
    for (int i = 0; i < devices.size(); ++i)
    {
      if (devices.at(i) == deviceName)
      {
        return i;
      }
    }

    // if none of the cameras match, return the first one
    return 0;
  }

  return deviceID;
}


QString CameraInfo::getFormat(int deviceID, int formatID)
{
  QStringList formats;

  getVideoFormats(deviceID, formats);

  if(formats.size() > formatID)
  {
    return formats.at(formatID);
  }
  else if(!formats.empty())
  {
    return formats.at(0);
  }
  return "No_camera";
}


QSize CameraInfo::getResolution(int deviceID, int formatID, int resolutionID)
{
  std::unique_ptr<QCamera> camera = loadCamera(deviceID);
  if (camera != nullptr)
  {
    QString format = getFormat(deviceID, formatID);
    QCameraViewfinderSettings viewSettings;
    viewSettings.setPixelFormat(stringToPixelFormat(format));
    QList<QSize> supportedResolutions = camera->supportedViewfinderResolutions(viewSettings);
    camera->unload();

    if(supportedResolutions.size() > resolutionID)
    {
      return supportedResolutions.at(resolutionID);
    }
    else if(!supportedResolutions.empty())
    {
      supportedResolutions.at(0);
    }
  }

  return QSize(0,0);
}


int CameraInfo::getFramerate(int deviceID, int formatID, int resolutionID, int framerateID)
{
  std::unique_ptr<QCamera> camera = loadCamera(deviceID);
  if (camera != nullptr)
  {
    QString format = getFormat(deviceID, formatID);
    QCameraViewfinderSettings viewSettings;
    viewSettings.setPixelFormat(stringToPixelFormat(format));
    QList<QSize> supportedResolutions = camera->supportedViewfinderResolutions(viewSettings);

    if (supportedResolutions.size() > resolutionID)
    {
      viewSettings.setResolution(supportedResolutions.at(resolutionID));
      QList<QCamera::FrameRateRange> framerates = camera->supportedViewfinderFrameRateRanges(viewSettings);
      camera->unload();

      if (framerates.size() > framerateID)
      {
        return framerates.at(framerateID).maximumFrameRate;
      }
      else if (!framerates.empty())
      {
        return framerates.at(0).maximumFrameRate;
      }
    }
    else if(!supportedResolutions.empty())
    {
      viewSettings.setResolution(supportedResolutions.at(0));
      QList<QCamera::FrameRateRange> framerates = camera->supportedViewfinderFrameRateRanges(viewSettings);
      camera->unload();

      if (!framerates.empty())
      {
        return framerates.at(0).maximumFrameRate;
      }
    }

    camera->unload();
  }

  return 0;
}

void CameraInfo::getCameraOptions(std::vector<SettingsCameraFormat>& options, int deviceID)
{
  if (deviceID != -1)
  {
    std::unique_ptr<QCamera> camera = loadCamera(deviceID);
    QStringList devices = getDeviceList();

    if (camera != nullptr)
    {
      QCameraViewfinderSettings viewSettings;
      QList<QVideoFrame::PixelFormat> p_formats = camera->supportedViewfinderPixelFormats();
      QStringList formats;
      getAllowedFormats(p_formats, formats);

      for (int formatID = 0; formatID < formats.size(); ++formatID)
      {
        viewSettings = QCameraViewfinderSettings{};
        viewSettings.setPixelFormat(stringToPixelFormat(formats.at(formatID)));
        QList<QSize> resolutions  = camera->supportedViewfinderResolutions(viewSettings);

        for (int resolutionID = 0; resolutionID < resolutions.size(); ++resolutionID)
        {
          viewSettings.setResolution(resolutions.at(resolutionID));
          QList<QCamera::FrameRateRange> framerates = camera->supportedViewfinderFrameRateRanges(viewSettings);

          for (int framerateID = 0; framerateID < framerates.size(); ++framerateID)
          {
            if (framerates.at(framerateID).maximumFrameRate >= 24.0)
            {
              options.push_back({devices.at(deviceID),                                         deviceID,
                                 formats.at(formatID),                                         formatID,
                                 resolutions.at(resolutionID),                                 resolutionID,
                                 QString::number(framerates.at(framerateID).maximumFrameRate), framerateID});
            }
          }
        }
      }

      camera->unload();
    }
  }
}


void CameraInfo::getAllowedFormats(QList<QVideoFrame::PixelFormat>& p_formats,
                                   QStringList& allowedFormats)
{
  for (int i = 0; i < p_formats.size() ; ++i)
  {
    for (int j = 0; j < kvazzupFormats.size(); ++j)
    {
      if (p_formats.at(i) == kvazzupFormats.at(j))
      {
        allowedFormats.push_back(pixelFormatStrings.at(p_formats.at(i)));
      }
    }
  }
}
