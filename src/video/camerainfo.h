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
  void getFramerates(int deviceID, QString format, int resolutionID);

  QString videoFormatToString(QVideoFrame::PixelFormat format);

private:

  std::unique_ptr<QCamera> loadCamera(int deviceID);

};
