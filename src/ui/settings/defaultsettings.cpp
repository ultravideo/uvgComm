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
  if (mic->getDeviceList().empty())
  {
    Logger::getLogger()->printWarning(this, "No mic found");
    settings_.setValue(SettingsKey::audioDevice,        "No microphone");
    settings_.setValue(SettingsKey::audioDeviceID,      -1);
  }
  else if (!validateAudioSettings() || settings_.value(SettingsKey::audioDeviceID).toInt() == -1)
  {
    Logger::getLogger()->printWarning(this, "Did not find valid audio settings, using defaults");
    setDefaultAudioSettings(mic);
  }

  if (cam->getDeviceList().empty())
  {
    Logger::getLogger()->printWarning(this, "No cam found");
    settings_.setValue(SettingsKey::videoDevice,        "No Camera");
    settings_.setValue(SettingsKey::videoDeviceID,      -1);
  }
  else  if (!validateVideoSettings() || settings_.value(SettingsKey::videoDeviceID).toInt() == -1)
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
                                SettingsKey::audioSelectiveMuting,
                                SettingsKey::audioMutingPeriod,
                                SettingsKey::audioMutingThreshold,
                                SettingsKey::audioDenoise,
                                SettingsKey::audioDereverb,
                                SettingsKey::audioAGC};

  return checkSettingsList(settings_, neededSettings);
}


bool DefaultSettings::validateVideoSettings()
{
  // video/DeviceID and video/Device are handled separately

  const QStringList neededSettings = {SettingsKey::videoResolutionWidth,
                                      SettingsKey::videoResolutionHeight,
                                      SettingsKey::videoInputFormat,
                                      SettingsKey::videoYUVThreads,
                                      SettingsKey::videoRGBThreads,
                                      SettingsKey::videoOpenHEVCThreads,
                                      SettingsKey::videoOHParallelization,
                                      SettingsKey::videoFramerateNumerator,
                                      SettingsKey::videoFramerateDenominator,
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
  QStringList devices = mic->getDeviceList();

  // should be checked earlier
  if (devices.empty())
  {
    Logger::getLogger()->printProgramError(this, "Devices should not be empty!");
    return;
  }

  int deviceID = -1;
  QString deviceName = "Error";

  // choose something in case nothing has been chosen
  if (settings_.value(SettingsKey::audioDevice).isNull() ||
      settings_.value(SettingsKey::audioDeviceID).isNull())
  {
    // TODO: Choose device based on which supports sample rate of 48000
    deviceID = 0;
    deviceName = devices.at(deviceID);
  }
  else
  {
    /* Here we try to improve our deviceID based on deviceName in case
     * the devices have changed */
    deviceID = settings_.value(SettingsKey::audioDeviceID).toInt();
    deviceName = settings_.value(SettingsKey::audioDevice).toString();

    deviceID = getMostMatchingDeviceID(devices, deviceName, deviceID);

    deviceName = devices.at(deviceID);
  }

  settings_.setValue(SettingsKey::audioDevice,        deviceName);
  settings_.setValue(SettingsKey::audioDeviceID,      deviceID);

  // TODO: Choose rest of the audio parameters such as samplerate (and maybe codec)

  settings_.setValue(SettingsKey::audioBitrate,         24000);
  settings_.setValue(SettingsKey::audioComplexity,      10);
  settings_.setValue(SettingsKey::audioSignalType,      "Auto");
  settings_.setValue(SettingsKey::audioAEC,             1);
  settings_.setValue(SettingsKey::audioAECDelay,        120);
  settings_.setValue(SettingsKey::audioAECFilterLength, 250);
  settings_.setValue(SettingsKey::audioMutingPeriod,    350);
  settings_.setValue(SettingsKey::audioMutingThreshold, 10);
  settings_.setValue(SettingsKey::audioSelectiveMuting, 1);

  settings_.setValue(SettingsKey::audioDenoise,         1);
  settings_.setValue(SettingsKey::audioDereverb,        1);
  settings_.setValue(SettingsKey::audioAGC,             0);
}


void DefaultSettings::setDefaultVideoSettings(std::shared_ptr<CameraInfo> cam)
{
  // Try to guess best resolution and thread distribution based on available threads
  unsigned int threads = std::thread::hardware_concurrency();
  if (threads <= 4)
  {
    /* Kvazaar wants the most threads, but some should be left for other components
     * to avoid too much competition for thread run time. The OWF options increases
     * latency, but is needed with high resolution in machines with low power cores.
     *
     * The openHEVC may require more threads if the incoming resolution is high,
     * but usually one is enough. The OpenHEVC parallelization mode slice is the best for latency,
     * but when receiving high resolution, "Frame and Slice" may be needed
     * (TODO: mode selection should be done based on received resolution to minimize latency).
     *
     * YUV and RGB conversions should be able to handle everything expect 4K with one thread. */
    settings_.setValue(SettingsKey::videoKvzThreads, threads);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 1);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 0);
  }
  else if (threads <= 8)
  {
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 1);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 2);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 0);
  }
  else if (threads <= 16)
  {
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 2);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 4);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 0);
  }
  else if (threads <= 24)
  {
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 2);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 6);
    settings_.setValue(SettingsKey::videoOHParallelization, "Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 1);
    settings_.setValue(SettingsKey::videoRGBThreads, 1);
    settings_.setValue(SettingsKey::videoOWF, 1);
  }
  else
  {
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 3);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 8);
    settings_.setValue(SettingsKey::videoOHParallelization, "Frame and Slice");
    settings_.setValue(SettingsKey::videoYUVThreads, 2);
    settings_.setValue(SettingsKey::videoRGBThreads, 2);
    settings_.setValue(SettingsKey::videoOWF, 2);
  }

#ifdef NDEBUG
  // The complexity guess is based on testing and is subject to changes
  uint64_t cpuPower = threads*CC_TRIVIAL/2;
#else
  // Use a lighter option in debug mode
  uint64_t cpuPower = threads*CC_TRIVIAL/16;
#endif

  SettingsCameraFormat format = selectBestCameraFormat(cam, cpuPower);

  settings_.setValue(SettingsKey::videoDevice,          format.deviceName);
  settings_.setValue(SettingsKey::videoDeviceID,        format.deviceID);

  // select best camera/format here
  settings_.setValue(SettingsKey::videoInputFormat,     format.format);
  settings_.setValue(SettingsKey::videoResolutionWidth,  format.resolution.width());
  settings_.setValue(SettingsKey::videoResolutionHeight, format.resolution.height());

  int32_t numerator = 0;
  int32_t denominator = 0;

  convertFramerate(format.framerate, numerator, denominator);

  settings_.setValue(SettingsKey::videoFramerateNumerator,       numerator);
  settings_.setValue(SettingsKey::videoFramerateDenominator,     denominator);

  settings_.setValue(SettingsKey::videoOpenGL, 0); // TODO: When can we enable this?
  settings_.setValue(SettingsKey::videoQP, 32);

  // video calls work better with high intra period
  settings_.setValue(SettingsKey::videoIntra, 320);
  settings_.setValue(SettingsKey::videoTiles, 0);
  settings_.setValue(SettingsKey::videoSlices, 0); // TODO: Fix slices and enable tiles
  settings_.setValue(SettingsKey::videoWPP, 1);
  settings_.setValue(SettingsKey::videoVPS, 1);
  settings_.setValue(SettingsKey::videoOBAClipNeighbours, 0);
  settings_.setValue(SettingsKey::videoScalingList, 0);
  settings_.setValue(SettingsKey::videoLossless, 0); // very CPU intensive
  settings_.setValue(SettingsKey::videoMVConstraint, "none");
  settings_.setValue(SettingsKey::videoQPInCU, 0);
  settings_.setValue(SettingsKey::videoVAQ, "disabled");

  settings_.setValue(SettingsKey::videoRCAlgorithm, "lambda");

  // use resolution and framerate to determine the best bit rate
  uint64_t formatComplexity = calculateComplexity(format.resolution,
                                                  format.framerate.toDouble());

  if (formatComplexity < CC_TRIVIAL)
  {
    settings_.setValue(SettingsKey::videoTileDimensions, "2x2");
    settings_.setValue(SettingsKey::videoBitrate, 250000); // 250 kbit/s
#ifdef NDEBUG
    settings_.setValue(SettingsKey::videoPreset, "fast");
#else
     settings_.setValue(SettingsKey::videoPreset, "ultrafast");
#endif
  }
  else if (formatComplexity < CC_EASY)
  {
    //settings_.setValue(SettingsKey::videoTiles, 0);
    settings_.setValue(SettingsKey::videoTileDimensions, "2x2");
    settings_.setValue(SettingsKey::videoBitrate, 500000); // 500 kbit/s
    settings_.setValue(SettingsKey::videoPreset, "faster");
  }
  else if (formatComplexity < CC_MEDIUM)
  {
    //settings_.setValue(SettingsKey::videoTiles, 1);
    settings_.setValue(SettingsKey::videoTileDimensions, "4x4");
    settings_.setValue(SettingsKey::videoBitrate, 1000000); // 1 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "veryfast");
  }
  else if (formatComplexity < CC_COMPLEX)
  {
    //settings_.setValue(SettingsKey::videoTiles, 1);
    settings_.setValue(SettingsKey::videoTileDimensions, "8x8");
    settings_.setValue(SettingsKey::videoBitrate, 3000000); // 3 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "superfast");
  }
  else
  {
    //settings_.setValue(SettingsKey::videoTiles, 1);
    settings_.setValue(SettingsKey::videoTileDimensions, "16x16");
    settings_.setValue(SettingsKey::videoBitrate, 6000000); // 6 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "ultrafast");
  }

#ifndef NDEBUG
  settings_.setValue(SettingsKey::videoPreset, "ultrafast");
#endif
}


void DefaultSettings::setDefaultCallSettings()
{}


SettingsCameraFormat DefaultSettings::selectBestCameraFormat(std::shared_ptr<CameraInfo> cam,
                                                             uint32_t complexity)
{
  // Note: the current implementation does not go through all the devices to
  // find the best format, just the first one
  if (!settings_.value(SettingsKey::videoDeviceID).isNull() &&
      settings_.value(SettingsKey::videoDeviceID) != "")
  {
    int deviceID = settings_.value(SettingsKey::videoDeviceID).toInt();
    deviceID = getMostMatchingDeviceID(cam->getDeviceList(),
                                       settings_.value(SettingsKey::videoDevice).toString(),
                                       deviceID);

    return selectBestDeviceFormat(cam, deviceID, complexity);
  }

  return selectBestDeviceFormat(cam, 0, complexity);
}


SettingsCameraFormat DefaultSettings::selectBestDeviceFormat(std::shared_ptr<CameraInfo> cam,
                                                             int deviceID, uint32_t complexity)
{
  // point system for best format
  SettingsCameraFormat bestOption = {"No option found", deviceID, "No option", {}, 0};
  uint64_t highestValue = 0;
  uint64_t lowestComplexity = CC_EXTREME;
  SettingsCameraFormat lowestComplexityOption = bestOption;

  std::vector<SettingsCameraFormat> options;
  cam->getCameraOptions(options, deviceID);

  for (auto& option: options)
  {
    uint64_t points = calculatePoints(option.format, option.resolution,
                                      option.framerate.toDouble());
    uint64_t optionComplexity = calculateComplexity(option.resolution,
                                                    option.framerate.toDouble());

    if (optionComplexity <= complexity)
    {
      if (points > highestValue)
      {
        highestValue = points;
        bestOption = option;
      }
    }
    else if (optionComplexity < lowestComplexity)
    {
      // try to find the lowest complexity option in case none of the options are suitable
      lowestComplexityOption = option;
      lowestComplexity = optionComplexity;
    }
  }

  // use lowest complexity option in case none are low complexity enough
  if (bestOption.deviceName == "No option found")
  {
    if (lowestComplexityOption.deviceName != "No option found")
    {
      Logger::getLogger()->printWarning(this, "Using lowest complexity option since none were suitable");
      bestOption = lowestComplexityOption;
    }
    else
    {
       Logger::getLogger()->printError(this, "No options were found!");
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

  if (format == "YUV420P")
  {
    formatPoints = 4; // no conversion needed for encoder
  }
  else if (format == "NV12"    ||
           format == "NV21"    ||
           format == "YUV422P" ||
           format == "UYVY"    ||
           format == "YUYV")
  {
    formatPoints = 3; // other YUV formats
  }
  else if (format == "RGB32" ||
           format == "ARGB32" ||
           format == "BGRA32" ||
           format == "ABGR"   ||
           format == "RGB24"  ||
           format == "BGR24")
  {
    formatPoints = 2; // RGB formats
  }
  else if (format == "MJPG")
  {
    formatPoints = 1; // has been encoded, so we try to avoid this
  }

  // try to use fps values between 30 and 60, since these offer good
  if (fps < 30.0 || 61.0 < fps)
  {
    return (resolution.width()*resolution.height() + (int)fps)*10 + formatPoints;
  }

  return (resolution.width()*resolution.height() + (int)fps)*100 + formatPoints;
}


uint64_t DefaultSettings::calculateComplexity(QSize resolution, double fps)
{
  return resolution.width()*resolution.height()*fps;
}
