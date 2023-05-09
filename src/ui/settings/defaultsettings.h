#pragma once

#include "camerainfo.h"

#include <QObject>
#include <QSettings>

#include <memory>

class CameraInfo;
class MicrophoneInfo;

enum ComplexityClass {CC_TRIVIAL = 10000000,   // 480p30,  10m,  almost any computer can achieve this
                      CC_EASY    = 20000000,   // 480p60,  20m,  most computers can achieve
                      CC_MEDIUM  = 30000000,   // 720p30,  30m,  average computers can achieve this
                      CC_COMPLEX = 70000000,   // 1080p30, 70m,  good computers can achieve this
                      CC_EXTREME = 300000000}; // 2160p30, 300m, only the best computers can achieve this

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

  void selectDefaultCamera(std::shared_ptr<MicrophoneInfo> mic);

  SettingsCameraFormat selectBestCameraFormat(std::shared_ptr<CameraInfo> cam, uint32_t complexity);
  SettingsCameraFormat selectBestDeviceFormat(std::shared_ptr<CameraInfo> cam, int deviceID, uint32_t complexity);
  uint64_t calculatePoints(QString format, QSize resolution, double fps);

  uint64_t calculateComplexity(QSize resolution, double fps);

  QSettings settings_;
};


