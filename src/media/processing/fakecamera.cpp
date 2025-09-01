#include "FakeCamera.h"

#include "logger.h"

#include <QString>

const QString FILENAME = "FakeCamera.yuv";
const int WIDTH = 1280;
const int HEIGHT = 720;
const int FRAMERATE = 30;


FakeCamera::FakeCamera(QString id, StatisticsInterface* stats,
                       std::shared_ptr<ResourceAllocator> hwResources):
Filter(id, "Camera", stats, hwResources, DT_NONE, DT_YUV420VIDEO),
resolution_(WIDTH, HEIGHT),
framerate_(FRAMERATE),
running_(false)
{}


FakeCamera::~FakeCamera()
{
  uninit();
}


bool FakeCamera::init()
{
  file_.rename(FILENAME);
  file_.open(QIODevice::ReadOnly);

  return Filter::init();
}


void FakeCamera::uninit()
{
  if (file_.isOpen())
  {
    file_.close();
  }
}

void FakeCamera::start()
{
  running_ = true;
  Filter::start();
}

void FakeCamera::stop()
{
  running_ = false;
  uninit();
  Filter::stop();
}


void FakeCamera::updateSettings()
{
  resolution_ = QSize(WIDTH, HEIGHT);
  framerate_ = FRAMERATE;

  uninit();
  init();
}


void FakeCamera::process()
{
  int frameSize = resolution_.width() * resolution_.height() * 3 / 2;
  auto framePeriod = std::chrono::duration<double>(1.0 / framerate_);
  auto start = std::chrono::steady_clock::now();

  uint64_t frame_index = 0;

  while (running_)
  {
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
      Logger::getLogger()->printWarning("FakeCamera",
                                        "Not enough data in file");
      running_ = false;
      break;
    }

    std::unique_ptr<Data> newImage = initializeData(output_, DS_LOCAL);
    auto now = std::chrono::system_clock::now();
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


    // sleep until next frame
    ++frame_index;
    auto targetTime = start + frame_index * framePeriod;
    if (std::chrono::steady_clock::now() < targetTime)
    {
      std::this_thread::sleep_until(targetTime);
    }
    else
    {
      // We are late; consider logging a warning or handling it as needed
      Logger::getLogger()->printWarning("FakeCamera",
                                        "Processing is lagging behind");
    }
  }
}
