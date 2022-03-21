#pragma once

#include "camerainfo.h"

#include <QObject>
#include <QSettings>

#include <memory>

class CameraInfo;
class MicrophoneInfo;

class DefaultSettings : public QObject
{
  Q_OBJECT
public:
  DefaultSettings();

  void validateSettings(std::shared_ptr<MicrophoneInfo> mic,
                        std::shared_ptr<CameraInfo> cam);

  // these need to be updated in case the device changes
  void setDefaultAudioSettings(std::shared_ptr<MicrophoneInfo> mic);
  void setDefaultVideoSettings(std::shared_ptr<CameraInfo> cam);

private:

  bool validateAudioSettings();
  bool validateVideoSettings();
  bool validateCallSettings();


  void setDefaultCallSettings();

  SettingsCameraFormat selectBestCameraFormat(std::shared_ptr<CameraInfo> cam);
  SettingsCameraFormat selectBestDeviceFormat(std::shared_ptr<CameraInfo> cam, int deviceID);

  uint64_t calculatePoints(QSize resolution, int fps);

  QSettings settings_;
};


