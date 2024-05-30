#include "cameraformats.h"



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



QString resolutionToString(QSize resolution)
{
  return QString::number(resolution.width()) + "x" +
         QString::number(resolution.height());
}


QString videoFormatToString(QVideoFrameFormat::PixelFormat format)
{
  return pixelFormatStrings.at(format);
}


QVideoFrameFormat::PixelFormat stringToPixelFormat(QString format)
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


DataType stringToDatatype(QString format)
{
  if(format == "YUV420P")
  {
    return DT_YUV420VIDEO;
  }
  else if(format == "YUV422P")
  {
    return DT_YUV422VIDEO;
  }

  else if(format == "NV12")
  {
    return DT_NV12VIDEO;
  }
  else if(format == "NV21")
  {
    return DT_NV21VIDEO;
  }

  else if(format == "YUYV")
  {
    return DT_YUYVVIDEO;
  }
  else if(format == "UYVY")
  {
    return DT_UYVYVIDEO;
  }

  else if(format == "ARGB32")
  {
    return DT_ARGBVIDEO;
  }
  else if(format == "BGRA32")
  {
    return DT_BGRAVIDEO;
  }
  else if(format == "ABGR")
  {
    return DT_ABGRVIDEO;
  }
  else if (format == "RGB32")
  {
    return DT_RGB32VIDEO;
  }
  else if (format == "RGB24")
  {
    return DT_RGB24VIDEO;
  }
  else if (format == "BGR24")
  {
    return DT_BGRXVIDEO;
  }
  else if(format == "MJPG")
  {
#if QT_VERSION_MAJOR == 6 && QT_VERSION_MINOR >= 5
    // Qt 6.5 starts using ffmpeg as backend, resulting in actual mjpeg support
    return DT_MJPEGVIDEO;
#else
    // Qt versions before 6.5 seem to always give RGB32 when asked for motion jpeg
    return DT_RGB32VIDEO;
#endif
  }

  return DT_NONE;
}
