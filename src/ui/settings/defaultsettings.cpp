#include "defaultsettings.h"

#include "settingshelper.h"
#include "microphoneinfo.h"

#include "logger.h"
#include "settingskeys.h"

#include <thread>

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
  QStringList neededSettings = {SettingsKey::audioBitrate,
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
  // video/DeviceID and video/Device are no checked in case we don't have a camera

  const QStringList neededSettings = {SettingsKey::videoResultionWidth,
                                      SettingsKey::videoResultionHeight,
                                      SettingsKey::videoResolutionID,
                                      SettingsKey::videoInputFormat,
                                      SettingsKey::videoYUVThreads,
                                      SettingsKey::videoRGBThreads,
                                      SettingsKey::videoOpenHEVCThreads,
                                      SettingsKey::videoOHParallelization,
                                      SettingsKey::videoFramerateID,
                                      SettingsKey::videoFramerate,
                                      SettingsKey::videoOpenGL,
                                      SettingsKey::videoQP,

                                      SettingsKey::videoIntra,
                                      SettingsKey::videoSlices,
                                      SettingsKey::videoKvzThreads,
                                      SettingsKey::videoWPP,
                                      SettingsKey::videoOWF,
                                      SettingsKey::videoTiles,
                                      SettingsKey::videoTileDimensions,
                                      SettingsKey::videoVPS,
                                      SettingsKey::videoBitrate,
                                      SettingsKey::videoRCAlgorithm,
                                      SettingsKey::videoOBAClipNeighbours,
                                      SettingsKey::videoScalingList,
                                      SettingsKey::videoLossless,
                                      SettingsKey::videoMVConstraint,
                                      SettingsKey::videoQPInCU,
                                      SettingsKey::videoVAQ,
                                      SettingsKey::videoPreset};

  return checkSettingsList(settings_, neededSettings);
}


bool DefaultSettings::validateCallSettings()
{
  return true;
}


void DefaultSettings::setDefaultAudioSettings(std::shared_ptr<MicrophoneInfo> mic)
{
  if (settings_.value(SettingsKey::audioDevice).isNull() ||
      settings_.value(SettingsKey::audioDevice).toString() == "")
  {
    // TODO: Choose device based on which supports sample rate of 48000
    QStringList devices = mic->getDeviceList();

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
  unsigned int threads = std::thread::hardware_concurrency();

  // Try to guess best resolution and thread distribution based on available threads
  ComplexityClass cpuPower = CC_TRIVIAL;
  if (threads <= 4)
  {
    /* Kvazaar wants the most threads, but some should be left for other components
     * to avoid too much competition for thread run time. The OWF options increases
     * latency, but is needed with high resolution in machines with low power cores.
     *
     * The openHEVC may require more threads if the incoming resolution is high,
     * but usually one is enough. The OpenHEVC parallelization mode slice is the best for latency,
     * but when receiving high resolution, "Frame and Slice" may be needed
     * (selection should probably be done based on received resolution).
     *
     * YUV and RGB conversions should be able to handle everything expect 4K with one thread.
     *
     * The resolution guess is based on testing and is subject to changes */

    if (threads < 4)
    {
      cpuPower = CC_TRIVIAL;
    }
    else if (threads == 4)
    {
      cpuPower = CC_EASY;
    }

    settings_.setValue(SettingsKey::videoKvzThreads, threads);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 1);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 0);
  }
  else if (threads <= 8)
  {
    cpuPower = CC_MEDIUM;
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 1);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 2);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 0);
  }
  else if (threads <= 16)
  {
    cpuPower = CC_COMPLEX;
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 1);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 4);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 1);
  }
  else
  {
    cpuPower = CC_EXTREME;
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 2);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 8);
    settings_.setValue(SettingsKey::videoOHParallelization, "Frame and Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 2);
    settings_.setValue(SettingsKey::videoRGBThreads, 2);
    settings_.setValue(SettingsKey::videoOWF, 2);
  }

  SettingsCameraFormat format = selectBestCameraFormat(cam, cpuPower);

  settings_.setValue(SettingsKey::videoDevice,          format.deviceName);
  settings_.setValue(SettingsKey::videoDeviceID,        format.deviceID);

  // select best camera/format here
  settings_.setValue(SettingsKey::videoResolutionID,    format.resolutionID);
  settings_.setValue(SettingsKey::videoFramerateID,     format.framerateID);

  settings_.setValue(SettingsKey::videoInputFormat,     format.format);
  settings_.setValue(SettingsKey::videoResultionWidth,  format.resolution.width());
  settings_.setValue(SettingsKey::videoResultionHeight, format.resolution.height());
  settings_.setValue(SettingsKey::videoFramerate,       format.framerate);

  settings_.setValue(SettingsKey::videoOpenGL, 0); // TODO: When can we enable this?
  settings_.setValue(SettingsKey::videoQP, 32);

  settings_.setValue(SettingsKey::videoIntra, 64);

  // TODO: This should be enabled when the performance is good enough
  settings_.setValue(SettingsKey::videoSlices, 0);

  settings_.setValue(SettingsKey::videoWPP, 1);
  settings_.setValue(SettingsKey::videoTiles, 0);
  settings_.setValue(SettingsKey::videoTileDimensions, "2x2");
  settings_.setValue(SettingsKey::videoVPS, 1);
  settings_.setValue(SettingsKey::videoOBAClipNeighbours, 0);
  settings_.setValue(SettingsKey::videoScalingList, 0);
  settings_.setValue(SettingsKey::videoLossless, 0); // very CPU intensive
  settings_.setValue(SettingsKey::videoMVConstraint, "none");
  settings_.setValue(SettingsKey::videoQPInCU, 0);
  settings_.setValue(SettingsKey::videoVAQ, "disabled");

  settings_.setValue(SettingsKey::videoRCAlgorithm, "lambda");

  ComplexityClass formatComplexity = calculateComplexity(format.resolution,
                                                         format.framerate.toDouble());

  // use resolution to determine the best bit rate
  if (formatComplexity == CC_TRIVIAL)
  {
    settings_.setValue(SettingsKey::videoBitrate, 250000); // 250 kbit/s
    settings_.setValue(SettingsKey::videoPreset, "fast");
  }
  else if (formatComplexity == CC_EASY)
  {
    settings_.setValue(SettingsKey::videoBitrate, 500000); // 500 kbit/s
    settings_.setValue(SettingsKey::videoPreset, "faster");
  }
  else if (formatComplexity == CC_MEDIUM)
  {
    settings_.setValue(SettingsKey::videoBitrate, 1000000); // 1 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "veryfast");
  }
  else if (formatComplexity == CC_COMPLEX)
  {
    settings_.setValue(SettingsKey::videoBitrate, 3000000); // 3 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "superfast");
  }
  else
  {
    settings_.setValue(SettingsKey::videoBitrate, 6000000); // 6 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "ultrafast");
  }
}


void DefaultSettings::setDefaultCallSettings()
{}


SettingsCameraFormat DefaultSettings::selectBestCameraFormat(std::shared_ptr<CameraInfo> cam,
                                                             ComplexityClass complexity)
{
  // Note: the current implementation does not go through all the devices to
  // find the best format, just the first one
  if (!settings_.value(SettingsKey::videoDeviceID).isNull() &&
      settings_.value(SettingsKey::videoDeviceID) != "")
  {
    int deviceID = settings_.value(SettingsKey::videoDeviceID).toInt();
    deviceID = cam->getMostMatchingDeviceID(settings_.value(SettingsKey::videoDevice).toString(),
                                            deviceID);
    return selectBestDeviceFormat(cam, deviceID, complexity);
  }

  return selectBestDeviceFormat(cam, 0, complexity);
}


SettingsCameraFormat DefaultSettings::selectBestDeviceFormat(std::shared_ptr<CameraInfo> cam,
                                                             int deviceID, ComplexityClass complexity)
{
  // point system for best format

  SettingsCameraFormat bestOption = {"No camera found", deviceID, "No camera",
                                     -1, {}, -1, 0, -1};
  uint64_t highestValue = 0;

  std::vector<SettingsCameraFormat> options;
  cam->getCameraOptions(options, deviceID);

  for (auto& option: options)
  {
    if (calculateComplexity(option.resolution,
                            option.framerate.toDouble()) <= complexity)
    {
      uint64_t points = calculatePoints(option.format, option.resolution,
                                        option.framerate.toDouble());

      if (points > highestValue)
      {
        highestValue = points;
        bestOption = option;
      }
    }
  }

  QString resolution = QString::number(bestOption.resolution.width()) + "x" +
                       QString::number(bestOption.resolution.height());

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Selected the best format",
                                  {"Points", "Device", "Format", "Resolution", "Framerate"},
                                   {QString::number(highestValue), bestOption.deviceName,
                                    bestOption.format, resolution, bestOption.framerate});

  return bestOption;
}

uint64_t DefaultSettings::calculatePoints(QString format, QSize resolution, double fps)
{
  // we give much lower score to low fps values

  int formatPoints = 0;

  if (format == "YUYV")
  {
    formatPoints = 1;
  }
  else if (format == "MJPG")
  {
    formatPoints = 2;
  }
  else if (format == "YUV_420")
  {
    formatPoints = 3;
  }

  if (fps < 30.0)
  {
    return (resolution.width()*resolution.height()/10 + (int)fps) + formatPoints;
  }

  return resolution.width()*resolution.height() + (int)fps + formatPoints;
}


ComplexityClass DefaultSettings::calculateComplexity(QSize resolution, double fps)
{
  uint64_t complexityScore = resolution.width()*resolution.height()*fps;

  if (complexityScore <= CC_TRIVIAL)
  {
    return CC_TRIVIAL;
  }
  else if (complexityScore <= CC_EASY)
  {
    return CC_EASY;
  }
  else if (complexityScore <= CC_MEDIUM)
  {
    return CC_MEDIUM;
  }
  else if (complexityScore <= CC_COMPLEX)
  {
    return CC_COMPLEX;
  }

  return CC_EXTREME;
}
