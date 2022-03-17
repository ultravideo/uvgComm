#pragma once

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

private:

  bool validateAudioSettings();
  bool validateVideoSettings();
  bool validateCallSettings();

  void setDefaultAudioSettings(std::shared_ptr<MicrophoneInfo> mic);
  void setDefaultVideoSettings(std::shared_ptr<CameraInfo> cam);
  void setDefaultCallSettings();

  QSettings settings_;
};


