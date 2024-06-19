#include "camerainfo.h"

#include "cameraformats.h"

#include "logger.h"

#include <QMediaDevices>
#include <QVideoFrameFormat>



// these are supported by uvgComm (can be converted to YUV420), list mostly based on which formats libyuv supports
const QList<QVideoFrameFormat::PixelFormat> uvgCommFormats = {QVideoFrameFormat::Format_YUV420P,
                                                              QVideoFrameFormat::Format_YUV422P,
                                                              QVideoFrameFormat::Format_NV12,
                                                              QVideoFrameFormat::Format_NV21,
                                                              QVideoFrameFormat::Format_YUYV, // YUY2
                                                              QVideoFrameFormat::Format_UYVY,
                                                              QVideoFrameFormat::Format_ARGB8888,
                                                              QVideoFrameFormat::Format_BGRA8888,
                                                              QVideoFrameFormat::Format_ABGR8888,
                                                              QVideoFrameFormat::Format_RGBA8888, // RGB32
                                                              QVideoFrameFormat::Format_RGBX8888, // RGB24
                                                              QVideoFrameFormat::Format_BGRX8888,
#if !uvgComm_NO_JPEG || (QT_VERSION_MAJOR == 6 && QT_VERSION_MINOR <= 4)
                                                              QVideoFrameFormat::Format_Jpeg
#endif
};



CameraInfo::CameraInfo()
{}


QStringList CameraInfo::getDeviceList()
{
  QStringList list;

  const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();

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
    QList<QVideoFrameFormat::PixelFormat> p_formats;

    for(auto& formatOption : camera->cameraDevice().videoFormats())
    {
        if (!p_formats.contains(formatOption.pixelFormat()))
        {
            p_formats.push_back(formatOption.pixelFormat());
        }
    }

    getAllowedFormats(p_formats, formats);
  }
}


void CameraInfo::getFormatResolutions(int deviceID, QString format, QStringList &resolutions)
{
  resolutions.clear();

  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  if (camera != nullptr)
  {
    QList<QSize> supportedResolutions;

    for (auto& formatOption : camera->cameraDevice().videoFormats())
    {
      if (format == videoFormatToString(formatOption.pixelFormat()))
      {
          if (!supportedResolutions.contains(formatOption.resolution()) &&
              goodResolution(formatOption.resolution()))
          {
            supportedResolutions.push_back(formatOption.resolution());
          }
      }
    }

    for (int i = 0; i < supportedResolutions.size(); ++i)
    {
      resolutions.push_back(resolutionToString(supportedResolutions[i]));
    }
  }
}


void CameraInfo::getFramerates(int deviceID, QString format, QString resolution, QStringList &ranges)
{
  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  if (camera != nullptr)
  {
    for (auto& formatOption : camera->cameraDevice().videoFormats())
    {
      if (format == videoFormatToString(formatOption.pixelFormat()) &&
          resolutionToString(formatOption.resolution()) == resolution)
      {
        if (!ranges.contains(QString::number(formatOption.maxFrameRate())))
        {
          ranges.push_back(QString::number(formatOption.maxFrameRate()));
        }
      }
    }
  }
}


std::unique_ptr<QCamera> CameraInfo::loadCamera(int deviceID)
{
  const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
  if(deviceID == -1 || deviceID >= cameras.size())
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, "CameraInfo", 
                                    "Invalid deviceID for getVideoCapabilities");
    return std::unique_ptr<QCamera> (nullptr);
  }

  std::unique_ptr<QCamera> camera
      = std::unique_ptr<QCamera>(new QCamera(cameras.at(deviceID)));

  return camera;
}




QString CameraInfo::getFormat(int deviceID, QString format)
{
  QStringList formats;

  getVideoFormats(deviceID, formats);

  if(formats.contains(format))
  {
    return format;
  }
  else if(!formats.empty())
  {
    return formats.at(0);
  }

  return "No_camera";
}


QSize CameraInfo::getResolution(int deviceID, QString format, QString resolution)
{
  // here we can trust that the format exists in options

  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  if (camera != nullptr)
  {
    // try to find the exact match
    for (auto& formatOption : camera->cameraDevice().videoFormats())
    {
      if (videoFormatToString(formatOption.pixelFormat()) == format &&
          resolutionToString(formatOption.resolution()) ==  resolution)
      {
        printFormatOption(formatOption);

        return formatOption.resolution();
      }
    }

    // select first resolution for this format as backup
    for (auto& formatOption : camera->cameraDevice().videoFormats())
    {
      if (videoFormatToString(formatOption.pixelFormat()) == format)
      {
        printFormatOption(formatOption);

        return formatOption.resolution();
      }
    }
  }

  return QSize(0,0);
}


int CameraInfo::getFramerate(int deviceID, QString format, QString resolution, QString framerate)
{
  return getVideoFormat(deviceID, format, resolution, framerate).maxFrameRate();
}


QCameraFormat CameraInfo::getVideoFormat(int deviceID, QString format, QString resolution, QString framerate)
{
  if (deviceID != -1)
  {
    std::unique_ptr<QCamera> camera = loadCamera(deviceID);

    if (camera != nullptr)
    {
      QList<QCameraFormat> options = camera->cameraDevice().videoFormats();
      for (auto& formatOption : options)
      {
        if (format == videoFormatToString(formatOption.pixelFormat()) &&
            resolutionToString(formatOption.resolution()) == resolution &&
            QString::number(formatOption.maxFrameRate()) == framerate)
        {
          return formatOption;
        }
      }

      if (!options.empty())
      {
        return options.first();
      }
    }
  }

  return {};
}


void CameraInfo::getCameraOptions(std::vector<SettingsCameraFormat>& options, int deviceID)
{
  if (deviceID != -1)
  {
    std::unique_ptr<QCamera> camera = loadCamera(deviceID);

    if (camera != nullptr)
    {
      QList<QVideoFrameFormat::PixelFormat> p_formats;
      QList<QSize> supportedResolutions;
      QList<int> framerates;
      for (auto& formatOption : camera->cameraDevice().videoFormats())
      {
        if (!uvgCommFormats.contains(formatOption.pixelFormat()) ||
            !goodResolution(formatOption.resolution()))
        {
          continue;
        }

        if (!p_formats.contains(formatOption.pixelFormat()))
        {
          p_formats.push_back(formatOption.pixelFormat());
          supportedResolutions.clear();
        }

        if (!supportedResolutions.contains(formatOption.resolution()))
        {
          supportedResolutions.push_back(formatOption.resolution());
          framerates.clear();
        }

        framerates.push_back(formatOption.maxFrameRate());
        options.push_back({camera->cameraDevice().description(),
                         deviceID,
                         videoFormatToString(formatOption.pixelFormat()),
                         formatOption.resolution(),
                         QString::number(formatOption.maxFrameRate())});
      }
    }
  }
}


void CameraInfo::getAllowedFormats(QList<QVideoFrameFormat::PixelFormat> &p_formats,
                                   QStringList& allowedFormats)
{
  for (int i = 0; i < p_formats.size() ; ++i)
  {
    for (int j = 0; j < uvgCommFormats.size(); ++j)
    {
      if (p_formats.at(i) == uvgCommFormats.at(j))
      {
        allowedFormats.push_back(videoFormatToString(p_formats.at(i)));
      }
    }
  }
}


void CameraInfo::printFormatOption(QCameraFormat& formatOption) const
{
  QString framerate = QString::number(formatOption.minFrameRate()) + " -> " +
                      QString::number(formatOption.maxFrameRate());
  QString resolution = resolutionToString(formatOption.resolution());

  QString format = videoFormatToString(formatOption.pixelFormat());

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "CameraInfo", "Camera format option",
                                   {"Format", "Resolution", "Frame rate"},
                                   {format, resolution, framerate});
}


bool CameraInfo::goodResolution(QSize resolution)
{
  return resolution.width()%8 == 0 && resolution.height()%8 == 0;
}
