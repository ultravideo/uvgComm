#include "defaultsettings.h"

#include "settingshelper.h"
#include "microphoneinfo.h"

#include "logger.h"
#include "settingskeys.h"

DefaultSettings::DefaultSettings():
  settings_(settingsFile, settingsFileFormat)
{}


void DefaultSettings::validateSettings(std::shared_ptr<MicrophoneInfo> mic,
                                       std::shared_ptr<CameraInfo> cam)
{
  if (!validateAudioSettings())
  {
    Logger::getLogger()->printWarning(this, "Did not find valid audio settings, using defaults");
    setDefaultAudioSettings(mic);
  }

  if (!validateVideoSettings())
  {
    Logger::getLogger()->printWarning(this, "Did not find valid video settings, using defaults");
    setDefaultVideoSettings(cam);
  }

  if (!validateCallSettings())
  {
    Logger::getLogger()->printWarning(this, "Did not find valid call settings, using defaults");
    setDefaultCallSettings();
  }
}


bool DefaultSettings::validateAudioSettings()
{
  QStringList neededSettings = {SettingsKey::audioDevice,
                                SettingsKey::audioDeviceID,
                                SettingsKey::audioBitrate,
                                SettingsKey::audioComplexity,
                                SettingsKey::audioSignalType,
                                SettingsKey::audioAEC,
                                SettingsKey::audioAECDelay,
                                SettingsKey::audioAECFilterLength,
                                SettingsKey::audioDenoise,
                                SettingsKey::audioDereverb,
                                SettingsKey::audioAGC};

  return checkSettingsList(settings_, neededSettings);
}


bool DefaultSettings::validateVideoSettings()
{
  return true;
}


bool DefaultSettings::validateCallSettings()
{
  return true;
}


void DefaultSettings::setDefaultAudioSettings(std::shared_ptr<MicrophoneInfo> mic)
{
  QStringList devices = mic->getDeviceList();


  // TODO: CHoose device based on which sample rates they support
  if (!devices.empty())
  {
    settings_.setValue(SettingsKey::audioDevice,        devices.first());
    settings_.setValue(SettingsKey::audioDeviceID,      0);
  }
  else
  {
    settings_.setValue(SettingsKey::audioDevice,        "");
    settings_.setValue(SettingsKey::audioDeviceID,      -1);
    Logger::getLogger()->printError(this, "Did not find microphone!");
  }

  // TODO: Choose rest of the audio paramers such as samplerate (and maybe codec)

  settings_.setValue(SettingsKey::audioBitrate,         24000);
  settings_.setValue(SettingsKey::audioComplexity,      10);
  settings_.setValue(SettingsKey::audioSignalType,      "Auto");
  settings_.setValue(SettingsKey::audioAEC,             1);
  settings_.setValue(SettingsKey::audioAECDelay,        120);
  settings_.setValue(SettingsKey::audioAECFilterLength, 250);
  settings_.setValue(SettingsKey::audioDenoise,         1);
  settings_.setValue(SettingsKey::audioDereverb,        1);
  settings_.setValue(SettingsKey::audioAGC,             1);
}


void DefaultSettings::setDefaultVideoSettings(std::shared_ptr<CameraInfo> cam)
{

}


void DefaultSettings::setDefaultCallSettings()
{

}
