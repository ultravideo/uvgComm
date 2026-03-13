#include "fakecamera.h"

#include "logger.h"
#include "settingskeys.h"
#include "common.h"

#include <QString>

#include <chrono>
#include <thread>


FakeCamera::FakeCamera(QString id, StatisticsInterface* stats,
                       std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "File Camera", stats, hwResources, DT_NONE, DT_YUV420VIDEO),
file_(),
resolution_(0, 0),
framerate_(0),
running_(false),
rtpTimestamp_(initializeRtpTimestamp())
{}


FakeCamera::~FakeCamera()
{}


bool FakeCamera::init()
{
  updateSettings();

  running_ = true;

  return Filter::init();
}


void FakeCamera::start()
{
  running_ = true;

  Filter::start();
}


void FakeCamera::stop()
{
  running_ = false;

  Filter::stop();
}


void FakeCamera::updateSettings()
{
  bool enabled = settingEnabled(SettingsKey::videoFileEnabled);

  if (enabled)
  {
    if (file_.isOpen() && file_.fileName() != settingString(SettingsKey::videoFilename))
    {
      file_.close();
      file_.rename(settingString(SettingsKey::videoFilename));
      file_.open(QIODevice::ReadOnly);
    }
    else if (!file_.isOpen())
    {
      file_.setFileName(settingString(SettingsKey::videoFilename));
      if (!file_.open(QIODevice::ReadOnly))
      {
        Logger::getLogger()->printError(this, QString("Could not open video file %1").arg(file_.fileName()));
        return;
      }
    }

    resolution_ = QSize(settingValue(SettingsKey::videoFileResolutionWidth),
                        settingValue(SettingsKey::videoFileResolutionHeight));

    framerate_ = settingValue(SettingsKey::videoFileFramerate);

    sendTimer_.setSingleShot(false);
    sendTimer_.setInterval(1000/framerate_);
    connect(&sendTimer_, &QTimer::timeout, this, &FakeCamera::sendFrame);
    sendTimer_.start();
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Disabling fake camera filter");
    sendTimer_.stop();
    if (file_.isOpen())
    {
      file_.close();
    }
  }
}


void FakeCamera::process()
{
  if (!file_.isOpen() || framerate_ <= 0 || resolution_.isEmpty()) {
    Logger::getLogger()->printError(this, "FakeCamera not initialized properly");
    return;
  }

  const int frameSize = resolution_.width() * resolution_.height() * 3 / 2;
  int64_t frameIndex = 0;
  auto startTimeSteady = std::chrono::steady_clock::now();

  while (running_)
  {
    // Expected time for this frame
    auto expectedPts = frameIndex * 1000 / framerate_;
    auto expectedTimeSteady = startTimeSteady + std::chrono::milliseconds(expectedPts);

    // Read one frame
    QByteArray buffer = file_.read(frameSize);
    if (buffer.size() < frameSize) {
      file_.seek(0);
      buffer = file_.read(frameSize);
    }
    if (buffer.size() < frameSize) {
      Logger::getLogger()->printWarning("FakeCamera", "Not enough data in file even after rewind");
      break; // fatal, nothing left
    }

    // Build the frame
    std::unique_ptr<Data> newImage = initializeData(output_, DS_LOCAL);

    newImage->type = output_;
    newImage->data_size = frameSize;
    newImage->vInfo->width = resolution_.width();
    newImage->vInfo->height = resolution_.height();
    newImage->vInfo->framerateNumerator = framerate_;
    newImage->vInfo->framerateDenominator = 1;
    newImage->data = std::make_unique<uchar[]>(frameSize);
    memcpy(newImage->data.get(), buffer.data(), frameSize);

    newImage->creationTimestamp = clockNowMs();
    newImage->presentationTimestamp = newImage->creationTimestamp;

    // Calculate RTP timestamp according to RFC 3550
    rtpTimestamp_ = updateVideoRtpTimestamp(rtpTimestamp_, framerate_, 1);
    newImage->rtpTimestamp = rtpTimestamp_;

    sendOutput(std::move(newImage));
    frameIndex++;

    std::this_thread::sleep_until(expectedTimeSteady);
  }

  Logger::getLogger()->printNormal(this, "FakeCamera stopped");
}


void FakeCamera::sendFrame()
{
  wakeUp();
}
