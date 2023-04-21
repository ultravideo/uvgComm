#include "camerainfo.h"

#include "logger.h"

#include <QMediaDevices>
#include <QVideoFrameFormat>

const std::map<QVideoFrameFormat::PixelFormat, QString> pixelFormatStrings = {{QVideoFrameFormat::Format_Invalid, "INVALID"},

                                                                        {QVideoFrameFormat::Format_ARGB8888,  "ARGB32"},
                                                                        {QVideoFrameFormat::Format_ARGB8888_Premultiplied,
                                                                               "ARGB32_Premultiplied"},

                                                                        {QVideoFrameFormat::Format_XRGB8888,   "XRGB"},

                                                                        {QVideoFrameFormat::Format_BGRA8888,   "BGRA32"},
                                                                        {QVideoFrameFormat::Format_BGRA8888_Premultiplied,
                                                                               "BGRA32_Premultiplied"},

                                                                        {QVideoFrameFormat::Format_ABGR8888,   "ABGR"},
                                                                        {QVideoFrameFormat::Format_XBGR8888,   "XBGR"},

                                                                        {QVideoFrameFormat::Format_RGBA8888,   "RGB32"},

                                                                        {QVideoFrameFormat::Format_BGRX8888,   "BGR24"}, // also known as RAW
                                                                        {QVideoFrameFormat::Format_RGBX8888,   "RGB24"},

                                                                        {QVideoFrameFormat::Format_AYUV,                "AYUV444"},
                                                                        {QVideoFrameFormat::Format_AYUV_Premultiplied,  "AYUV444_Premultiplied"},

                                                                        {QVideoFrameFormat::Format_YUV422P,             "YUV422P"},
                                                                        {QVideoFrameFormat::Format_YUV420P,             "YUV420P"},

                                                                        {QVideoFrameFormat::Format_YV12,                "YV12"},
                                                                        {QVideoFrameFormat::Format_UYVY,                "UYVY"},
                                                                        {QVideoFrameFormat::Format_YUYV,                "YUYV"}, // also called YUY2

                                                                        {QVideoFrameFormat::Format_NV12,                "NV12"},
                                                                        {QVideoFrameFormat::Format_NV21,                "NV21"},

                                                                        {QVideoFrameFormat::Format_IMC1,                "IMC1"},
                                                                        {QVideoFrameFormat::Format_IMC2,                "IMC2"},
                                                                        {QVideoFrameFormat::Format_IMC3,                "IMC3"},
                                                                        {QVideoFrameFormat::Format_IMC4,                "IMC4"},

                                                                        {QVideoFrameFormat::Format_Y8,                  "Y8"},
                                                                        {QVideoFrameFormat::Format_Y16,                 "Y16"},

                                                                        {QVideoFrameFormat::Format_P010,                "P010"},
                                                                        {QVideoFrameFormat::Format_P016,                "P016"},

                                                                        {QVideoFrameFormat::Format_Jpeg,                "MJPG"}};

// these are supported by kvazzup (can be converted to YUV420), list mostly based on which formats libyuv supports
const QList<QVideoFrameFormat::PixelFormat> kvazzupFormats = {QVideoFrameFormat::Format_YUV420P,
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
                                                              QVideoFrameFormat::Format_Jpeg};



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
      if (format == pixelFormatStrings.at(formatOption.pixelFormat()))
      {
          if (!supportedResolutions.contains(formatOption.resolution()))
          {
            supportedResolutions.push_back(formatOption.resolution());
          }
      }
    }

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
    QList<QSize> supportedResolutions;
    for (auto& formatOption : camera->cameraDevice().videoFormats())
    {
      if (format == pixelFormatStrings.at(formatOption.pixelFormat()))
      {
        if (!supportedResolutions.contains(formatOption.resolution()))
        {
        supportedResolutions.push_back(formatOption.resolution());
        }

        if (supportedResolutions.size() - 1 == resolutionID)
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


QString CameraInfo::videoFormatToString(QVideoFrameFormat::PixelFormat format)
{
  return pixelFormatStrings.at(format);
}

QVideoFrameFormat::PixelFormat CameraInfo::stringToPixelFormat(QString format)
{
  for(auto& type : pixelFormatStrings)
  {
    if(type.second == format)
    {
      return type.first;
    }
  }

  return QVideoFrameFormat::PixelFormat::Format_Invalid;
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
    QList<QVideoFrameFormat::PixelFormat> p_formats;
    QList<QSize> supportedResolutions;
    for (auto& formatOption : camera->cameraDevice().videoFormats())
    {
      if (!p_formats.contains(formatOption.pixelFormat()))
      {
        p_formats.push_back(formatOption.pixelFormat());
      }

      if (p_formats.size() - 1 == formatID)
      {
        if (!supportedResolutions.contains(formatOption.resolution()))
        {
          supportedResolutions.push_back(formatOption.resolution());
        }

        if (supportedResolutions.size() - 1 == resolutionID)
        {
          return formatOption.resolution();
        }
      }
    }
  }

  return QSize(0,0);
}


int CameraInfo::getFramerate(int deviceID, int formatID, int resolutionID, int framerateID)
{
  return getVideoFormat(deviceID, formatID, resolutionID, framerateID).maxFrameRate();
}


QCameraFormat CameraInfo::getVideoFormat(int deviceID, int formatID, int resolutionID, int framerateID)
{
  if (deviceID != -1)
  {
    std::unique_ptr<QCamera> camera = loadCamera(deviceID);

    if (camera != nullptr)
    {
      QList<QVideoFrameFormat::PixelFormat> p_formats;
      QList<QSize> supportedResolutions;
      QList<int> framerates;
      QList<QCameraFormat> options = camera->cameraDevice().videoFormats();

      for (auto& formatOption : options)
      {
        if (!p_formats.contains(formatOption.pixelFormat()))
        {
          p_formats.push_back(formatOption.pixelFormat());
        }

        if (p_formats.size() - 1 == formatID)
        {
          if (!supportedResolutions.contains(formatOption.resolution()))
          {
            supportedResolutions.push_back(formatOption.resolution());
          }

          if (supportedResolutions.size() - 1 == resolutionID)
          {
            framerates.push_back(formatOption.maxFrameRate());

            if (framerates.size() - 1 == framerateID)
            {
                return formatOption;
            }
          }
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
        if (!kvazzupFormats.contains(formatOption.pixelFormat()))
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
                          pixelFormatStrings.at(formatOption.pixelFormat()),
                           (int)p_formats.size() - 1,
                          formatOption.resolution(),
                           (int)supportedResolutions.size() -1 ,
                          QString::number(formatOption.maxFrameRate()),
                           (int)framerates.size() -1});
      }
    }
  }
}


void CameraInfo::getAllowedFormats(QList<QVideoFrameFormat::PixelFormat> &p_formats,
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
