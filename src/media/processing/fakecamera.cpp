#include "FakeCamera.h"

#include "logger.h"
#include "settingskeys.h"
#include "common.h"

#include <QString>


FakeCamera::FakeCamera(QString id, StatisticsInterface* stats,
                       std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "File Camera", stats, hwResources, DT_NONE, DT_YUV420VIDEO),
file_(),
resolution_(0, 0),
framerate_(0)
{}


FakeCamera::~FakeCamera()
{}


bool FakeCamera::init()
{
  updateSettings();

  return Filter::init();
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
  int frameSize = resolution_.width() * resolution_.height() * 3 / 2;

  QByteArray buffer = file_.read(frameSize);
  if (buffer.size() < frameSize)
  {
    // End of file reached; you can loop
    file_.seek(0);
    buffer = file_.read(frameSize);
  }

  if (buffer.size() < frameSize)
  {
    // Still not enough data; exit the loop
    Logger::getLogger()->printWarning("FakeCamera", "Not enough data in file");
    return;
  }

  std::unique_ptr<Data> newImage = initializeData(output_, DS_LOCAL);
  auto now = std::chrono::steady_clock::now();
  newImage->creationTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  newImage->presentationTimestamp = newImage->creationTimestamp;

  newImage->type = output_;
  newImage->data_size = frameSize;
  newImage->vInfo->width = resolution_.width();
  newImage->vInfo->height = resolution_.height();
  newImage->vInfo->framerateNumerator = framerate_;
  newImage->vInfo->framerateDenominator = 1;
  newImage->data = std::make_unique<uchar[]>(frameSize);
  memcpy(newImage->data.get(), buffer.data(), frameSize);
  sendOutput(std::move(newImage));
}


void FakeCamera::sendFrame()
{
  wakeUp();
}
