#include "camerainfo.h"

#include <QCameraInfo>
#include <QDebug>


const std::map<QVideoFrame::PixelFormat, QString> pixelFormatStrings = {{QVideoFrame::Format_Invalid, "INVALID"},
                                                         {QVideoFrame::Format_ARGB32, "ARGB32"},
                                                         {QVideoFrame::Format_ARGB32_Premultiplied, "ARGB32_Premultiplied"},
                                                         {QVideoFrame::Format_RGB32, "RGB32"},
                                                         {QVideoFrame::Format_RGB24, "RGB24"},
                                                         {QVideoFrame::Format_RGB565, "RGB565"},
                                                         {QVideoFrame::Format_RGB555, "RGB555"},
                                                         {QVideoFrame::Format_ARGB8565_Premultiplied, "ARGB8565_Premultiplied"},
                                                         {QVideoFrame::Format_BGRA32, "BGRA32"},
                                                         {QVideoFrame::Format_BGRA32_Premultiplied, "BGRA32_Premultiplied"},
                                                         {QVideoFrame::Format_BGR32, "BGR32"},
                                                         {QVideoFrame::Format_BGR24, "BGR24"},
                                                         {QVideoFrame::Format_BGR565, "BGR565"},
                                                         {QVideoFrame::Format_BGR555, "BGR555"},
                                                         {QVideoFrame::Format_BGRA5658_Premultiplied, "BGRA5658_Premultiplied"},
                                                         {QVideoFrame::Format_AYUV444, "AYUV444"},
                                                         {QVideoFrame::Format_AYUV444_Premultiplied, "AYUV444_Premultiplied"},
                                                         {QVideoFrame::Format_YUV444, "YUV444"},
                                                         {QVideoFrame::Format_YUV420P, "YUV420P"},
                                                         {QVideoFrame::Format_YV12, "YV12"},
                                                         {QVideoFrame::Format_UYVY, "UYVY"},
                                                         {QVideoFrame::Format_YUYV, "YUYV"},
                                                         {QVideoFrame::Format_NV12, "NV12"},
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

  for(int i = 0; i < p_formats.size() ; ++i)
  {
    formats.push_back(pixelFormatStrings.at(p_formats.at(i)));
  }
}


void CameraInfo::getFormatResolutions(int deviceID, QString format, QStringList &resolutions)
{
  resolutions.clear();

  QCameraViewfinderSettings viewSettings;
  viewSettings.setPixelFormat(stringToPixelFormat(format));

  std::unique_ptr<QCamera> camera = loadCamera(deviceID);

  QList<QSize> supporteResolutions = camera->supportedViewfinderResolutions(viewSettings);
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


QString CameraInfo::videoFormatToString(QVideoFrame::PixelFormat format)
{
  return pixelFormatStrings.at(format);
}

QVideoFrame::PixelFormat CameraInfo::stringToPixelFormat(QString format)
{
  for(auto type : pixelFormatStrings)
  {
    if(type.second == format)
    {
      return type.first;
    }
  }

  return QVideoFrame::PixelFormat::Format_Invalid;
}



QSize CameraInfo::getResolution(int deviceID, int formatID, int resolutionID)
{

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

