#pragma once

#include <QStringList>
#include <QSize>
#include <QtMultimedia/QCamera>

#include <memory>

#include "deviceinfointerface.h"

// A helper class for settings.

class CameraInfo : public DeviceInfoInterface
{
public:
  CameraInfo();

  // returns a list of devices.
  virtual QStringList getDeviceList();

  // get formats for a device.
  void getVideoFormats(int deviceID, QStringList& formats);

  // get resolutions for a format.
  void getFormatResolutions(int deviceID, QString format, QStringList& resolutions);

  // get resolutions for a format.
  void getFramerates(int deviceID, QString format, int resolutionID, QStringList& ranges);

  QSize getResolution(int deviceID, int formatID, int resolutionID);

  QString getFormat(int deviceID, int formatID);

private:

#if QT_VERSION_MAJOR == 6
  QString videoFormatToString(QVideoFrameFormat::PixelFormat format);
  QVideoFrameFormat::PixelFormat stringToPixelFormat(QString format);
#else
  QString videoFormatToString(QVideoFrame::PixelFormat format);
  QVideoFrame::PixelFormat stringToPixelFormat(QString format);
#endif

  std::unique_ptr<QCamera> loadCamera(int deviceID);

};
