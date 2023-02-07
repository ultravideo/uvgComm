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

  const QStringList neededSettings = {SettingsKey::videoResultionWidth,
                                      SettingsKey::videoResultionHeight,
                                      SettingsKey::videoResolutionID,
                                      SettingsKey::videoInputFormat,
                                      SettingsKey::videoYUVThreads,
                                      SettingsKey::videoRGBThreads,
                                      SettingsKey::videoOpenHEVCThreads,
                                      SettingsKey::videoOHParallelization,
                                      SettingsKey::videoFramerateID,
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
  ComplexityClass cpuPower = CC_TRIVIAL;

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
     * (TODO: selection should probably be done based on received resolution).
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
  else if (threads <= 24)
  {
    settings_.setValue(SettingsKey::videoKvzThreads, threads - 2);
    settings_.setValue(SettingsKey::videoOpenHEVCThreads, 4);
    settings_.setValue(SettingsKey::videoOHParallelization, "Frame and Slice");
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
  if (threads < 4)
  {
    cpuPower = CC_TRIVIAL;
  }
  else if (threads == 4)
  {
    cpuPower = CC_EASY;
  }
  else if (threads <= 8)
  {
    cpuPower = CC_MEDIUM;
  }
  else if (threads <= 16)
  {
    cpuPower = CC_COMPLEX;
  }
  else
  {
    cpuPower = CC_EXTREME;
  }
#else
  // we always use the lightest option when using debug mode to get a more responsive program
  cpuPower = CC_TRIVIAL;
#endif

  SettingsCameraFormat format = selectBestCameraFormat(cam, cpuPower);

  settings_.setValue(SettingsKey::videoDevice,          format.deviceName);
  settings_.setValue(SettingsKey::videoDeviceID,        format.deviceID);

  // select best camera/format here
  settings_.setValue(SettingsKey::videoResolutionID,    format.resolutionID);
  settings_.setValue(SettingsKey::videoFramerateID,     format.framerateID);

  settings_.setValue(SettingsKey::videoInputFormat,     format.format);
  settings_.setValue(SettingsKey::videoResultionWidth,  format.resolution.width());
  settings_.setValue(SettingsKey::videoResultionHeight, format.resolution.height());

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
  ComplexityClass formatComplexity = calculateComplexity(format.resolution,
                                                         format.framerate.toDouble());

  if (formatComplexity == CC_TRIVIAL)
  {
    settings_.setValue(SettingsKey::videoTileDimensions, "2x2");
    settings_.setValue(SettingsKey::videoBitrate, 250000); // 250 kbit/s
#ifdef NDEBUG
    settings_.setValue(SettingsKey::videoPreset, "fast");
#else
     settings_.setValue(SettingsKey::videoPreset, "ultrafast");
#endif
  }
  else if (formatComplexity == CC_EASY)
  {
    //settings_.setValue(SettingsKey::videoTiles, 0);
    settings_.setValue(SettingsKey::videoTileDimensions, "2x2");
    settings_.setValue(SettingsKey::videoBitrate, 500000); // 500 kbit/s
    settings_.setValue(SettingsKey::videoPreset, "faster");
  }
  else if (formatComplexity == CC_MEDIUM)
  {
    //settings_.setValue(SettingsKey::videoTiles, 1);
    settings_.setValue(SettingsKey::videoTileDimensions, "4x4");
    settings_.setValue(SettingsKey::videoBitrate, 1000000); // 1 mbit/s
    settings_.setValue(SettingsKey::videoPreset, "veryfast");
  }
  else if (formatComplexity == CC_COMPLEX)
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
    deviceID = getMostMatchingDeviceID(cam->getDeviceList(),
                                       settings_.value(SettingsKey::videoDevice).toString(),
                                       deviceID);

    return selectBestDeviceFormat(cam, deviceID, complexity);
  }

  return selectBestDeviceFormat(cam, 0, complexity);
}


SettingsCameraFormat DefaultSettings::selectBestDeviceFormat(std::shared_ptr<CameraInfo> cam,
                                                             int deviceID, ComplexityClass complexity)
{
#ifdef __linux__
  // this is the only format that seems to work on Linux in Qt 5
  return {"Linux camera", deviceID, "RGB32", -1, {640, 480}, -1, "30", -1};

#else

  // point system for best format
  SettingsCameraFormat bestOption = {"No option found", deviceID, "No option",
                                     -1, {}, -1, 0, -1};
  uint64_t highestValue = 0;
  ComplexityClass lowestComplexity = CC_EXTREME;
  SettingsCameraFormat lowestComplexityOption = bestOption;

  std::vector<SettingsCameraFormat> options;
  cam->getCameraOptions(options, deviceID);

  for (auto& option: options)
  {
    uint64_t points = calculatePoints(option.format, option.resolution,
                                      option.framerate.toDouble());
    ComplexityClass optionComplexity = calculateComplexity(option.resolution,
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
#endif
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

  // try to use fps values between 30 and 60, since these are most widely supported
  if (fps < 30.0 || 61.0 < fps)
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
