#pragma once

#include <QStringList>
#include <QSize>
#include <QtMultimedia/QCamera>

#include <memory>

#include "deviceinfointerface.h"

struct SettingsCameraFormat
{
  QString deviceName;
  int deviceID;

  QString format;
  int formatID;
  QSize resolution;
  int resolutionID;

  QString framerate;
  int framerateID;
};

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

  // returns -1 if there are no devices
  int getMostMatchingDeviceID(QString deviceName, int deviceID);
  QString getFormat(int deviceID, int formatID);
  QSize   getResolution(int deviceID, int formatID, int resolutionID);
  int     getFramerate(int deviceID, int formatID, int resolutionID, int framerateID);


  void getCameraOptions(std::vector<SettingsCameraFormat>& options, int deviceID);

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
