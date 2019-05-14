#pragma once

#include <QStringList>
#include <QSize>
#include <QCamera>

#include <memory>


// A helper class for settings.

class CameraInfo
{
public:
  CameraInfo();

  // returns a list of devices. with devideID as the place in list
  QStringList getVideoDevices();

  // get formats for a device.
  // the for
  void getVideoFormats(int deviceID, QStringList& formats);

  // get resolutions for a format.
  void getFormatResolutions(int deviceID, QString format, QStringList& resolutions);

  // get resolutions for a format.
  void getFramerates(int deviceID, QString format, int resolutionID, QStringList& ranges);

  QSize getResolution(int deviceID, int formatID, int resolutionID);

  QString getFormat(int deviceID, int formatID);

private:

  QString videoFormatToString(QVideoFrame::PixelFormat format);
  QVideoFrame::PixelFormat stringToPixelFormat(QString format);

  std::unique_ptr<QCamera> loadCamera(int deviceID);

};
