#include "camerafilter.h"

#include "cameraframegrabber.h"
#include "statisticsinterface.h"

#include "settingskeys.h"

#include "common.h"
#include "logger.h"

#include <QSettings>
#include <QtMultimedia/QCameraInfo>
#include <QTime>

#include <chrono>
#include <thread>


CameraFilter::CameraFilter(QString id, StatisticsInterface *stats,
                           std::shared_ptr<HWResourceManager> hwResources):
  Filter(id, "Camera", stats, hwResources, DT_NONE, DT_RGB32VIDEO),
  camera_(nullptr),
  cameraFrameGrabber_(nullptr),
  framerate_(0),
  currentDeviceName_(""),
  currentDeviceID_(-1),
  currentInputFormat_(""),
  currentResolutionID_(-1),
  currentFramerateID_(-1)
{}


CameraFilter::~CameraFilter()
{
  if (camera_ != nullptr)
  {
    delete camera_;
  }
  if(cameraFrameGrabber_ != nullptr)
  {
    delete cameraFrameGrabber_;
  }
}


void CameraFilter::updateSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);

  QString deviceName = settings.value(SettingsKey::videoDevice).toString();
  int deviceID = settings.value(SettingsKey::videoDeviceID).toInt();
  QString inputFormat = settings.value(SettingsKey::videoInputFormat).toString();
  int resolutionID = settings.value(SettingsKey::videoResolutionID).toInt();
  int framerateID = settings.value(SettingsKey::videoFramerateID).toInt();

  if (deviceName != currentDeviceName_ ||
      deviceID != currentDeviceID_ ||
      inputFormat != currentInputFormat_ ||
      resolutionID != currentResolutionID_ ||
      framerateID != currentFramerateID_)
  {
    Logger::getLogger()->printNormal(this, "Updating camera settings since they have changed");

    // TODO: Just set the viewfinder settings instead for restarting camera.
    // Settings the viewfinder does not always restart camera.

    stop();
    uninit();
    init();
    start();
  }

  Filter::updateSettings();
}


void CameraFilter::uninit()
{
  stop();
  if (camera_ != nullptr)
  {
    delete camera_;
    camera_ = nullptr;
  }

  if (cameraFrameGrabber_ != nullptr)
  {
    delete cameraFrameGrabber_;
    cameraFrameGrabber_ = nullptr;
  }
}


bool CameraFilter::init()
{
  if (initialCameraSetup() && cameraSetup())
  {
    return Filter::init();
  }
  return false;
}


void CameraFilter::run()
{
  Filter::run();
}

bool CameraFilter::initialCameraSetup()
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

  if(cameras.size() == 0)
  {
    Logger::getLogger()->printWarning(this, "No camera found!");
    return false;
  }

  QSettings settings(settingsFile, settingsFileFormat);
  currentDeviceName_ = settings.value(SettingsKey::videoDevice).toString();
  currentDeviceID_ = settings.value(SettingsKey::videoDeviceID).toInt();

#ifndef __linux__
  if (settings.value(SettingsKey::videoInputFormat).toString() == "")
  {
    Logger::getLogger()->printWarning(this, "Camera has no formats");
    return false;
  }
#endif

  // if the deviceID has changed
  if (currentDeviceID_ >= cameras.size() ||
      cameras[currentDeviceID_].description() != currentDeviceName_)
  {
    // search for device with same name
    for(int i = 0; i < cameras.size(); ++i)
    {
      if(cameras.at(i).description() == currentDeviceName_)
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Found the camera.", {"Description", "ID"},
                   {cameras.at(i).description(), QString::number(i)});
        currentDeviceID_ = i;
        break;
      }
    }
    // previous camera could not be found, use first.
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Did not find the camera. Using first.", 
                                    {"Settings camera", "First camera"},
               {currentDeviceName_, cameras.at(0).description()});
    currentDeviceID_ = 0;
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initiating Qt camera", {"ID"},
              {QString::number(currentDeviceID_)});

  if (currentDeviceID_ < cameras.size())
  {
    camera_ = new QCamera(cameras.at(currentDeviceID_));
    cameraFrameGrabber_ = new CameraFrameGrabber();

#ifndef __linux__
    camera_->load();
#endif
    return true;
  }

  return false;
}


bool CameraFilter::cameraSetup()
{
  Q_ASSERT(camera_ && cameraFrameGrabber_);

  if (camera_ && cameraFrameGrabber_)
  {
    QSettings settings(settingsFile, settingsFileFormat);
#ifndef __linux__

    int waitRoundMS = 5;
    int totalWaitTime = 1000;
    int currentWaitTime = 0;
    while(camera_->state() != QCamera::LoadedState)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(waitRoundMS));
      currentWaitTime += waitRoundMS;
      if (currentWaitTime >= totalWaitTime)
      {
        Logger::getLogger()->printWarning(this, "Loading camera did not succeed.");
        return false;
      }
    }
#endif

    camera_->setViewfinder(cameraFrameGrabber_);

    connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(handleFrame(QVideoFrame)));

    QCameraViewfinderSettings viewSettings = camera_->viewfinderSettings();

    currentInputFormat_ = settings.value(SettingsKey::videoInputFormat).toString();

#ifdef __linux__
    viewSettings.setPixelFormat(QVideoFrame::Format_RGB32);
    output_ = RGB32VIDEO;
#else
    if(currentInputFormat_ == "MJPG")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_Jpeg);
      output_ = DT_RGB32VIDEO;
    }
    else if(currentInputFormat_ == "RGB32")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_RGB32);
      output_ = DT_RGB32VIDEO;
    }
    else if(currentInputFormat_ == "YUV420P")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_YUV420P);
      output_ = DT_YUV420VIDEO;
    }
    else if(currentInputFormat_ == "YUYV")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_YUYV);
      output_ = DT_YUYVVIDEO;
    }
    else
    {
      Logger::getLogger()->printError(this, "Input format not supported", 
                                      {"Format"}, {currentInputFormat_});
      viewSettings.setPixelFormat(QVideoFrame::Format_Invalid);
      output_ = DT_NONE;
      return false;
    }

#endif

#ifndef __linux__
    QList<QSize> resolutions = camera_->supportedViewfinderResolutions(viewSettings);

    currentResolutionID_ = settings.value(SettingsKey::videoResolutionID).toInt();
    if(resolutions.size() > currentResolutionID_ && !resolutions.empty())
    {
      QSize resolution = resolutions.at(currentResolutionID_);
      viewSettings.setResolution(QSize(resolution.width(),
                                       resolution.height()));
    }
    else {
      viewSettings.setResolution(QSize(0,0));
    }
#else
    viewSettings.setResolution(QSize(640, 480));
#endif

    QList<QCamera::FrameRateRange> framerates = camera_->supportedViewfinderFrameRateRanges(viewSettings);

    currentFramerateID_ = settings.value(SettingsKey::videoFramerateID).toInt();

#ifndef __linux__
    if (!framerates.empty())
    {
      if(currentFramerateID_ < framerates.size() && currentFramerateID_ >= 0)
      {
        viewSettings.setMaximumFrameRate(framerates.at(currentFramerateID_).maximumFrameRate);
        viewSettings.setMinimumFrameRate(framerates.at(currentFramerateID_).minimumFrameRate);
      }
      else {
        viewSettings.setMaximumFrameRate(framerates.back().maximumFrameRate);
        viewSettings.setMinimumFrameRate(framerates.back().minimumFrameRate);
      }
    }
#else
    viewSettings.setMaximumFrameRate(30);
    viewSettings.setMinimumFrameRate(30);
#endif

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Using the following camera settings.",
               {"Format", "Resolution", "Framerate"}, {currentInputFormat_,
               QString::number(viewSettings.resolution().width()) + "x" + QString::number(viewSettings.resolution().height()),
               QString::number(viewSettings.minimumFrameRate()) + " to " + QString::number(viewSettings.maximumFrameRate())});

    getStats()->videoInfo(viewSettings.maximumFrameRate(),
                          viewSettings.resolution());
    framerate_ = viewSettings.maximumFrameRate();

    camera_->setViewfinderSettings(viewSettings);
    camera_->start();

    Logger::getLogger()->printImportant(this, "Starting camera.");
  }
  else
  {
    return false;
  }
  return true;
}


void CameraFilter::start()
{
  Filter::start();
  if(camera_)
  {
    camera_->start();
  }
}


void CameraFilter::stop()
{
  if(camera_)
  {
    camera_->stop();
  }
  Filter::stop();
}


void CameraFilter::handleFrame(const QVideoFrame &frame)
{
  frameMutex_.lock();
  frames_.push_back(frame);
  frameMutex_.unlock();
  wakeUp();
}


void CameraFilter::process()
{
  if(frames_.empty())
  {
    Logger::getLogger()->printWarning(this, "No frame.");
    return;
  }

  while(!frames_.empty())
  {
    QVideoFrame frame = frames_.front();
    frameMutex_.lock();
    frames_.pop_front();
    frameMutex_.unlock();

    // capture the frame data
    std::unique_ptr<Data> newImage = initializeData(output_, DS_LOCAL);
    newImage->presentationTime = QDateTime::currentMSecsSinceEpoch();
    newImage->type = output_;

    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QAbstractVideoBuffer::ReadOnly);

    newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes()]);
    uchar *bits = cloneFrame.bits();

    memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes());
    newImage->data_size = cloneFrame.mappedBytes();

    // kvazaar requires divisable by 8 resolution
    newImage->vInfo->width = cloneFrame.width() - cloneFrame.width()%8;
    newImage->vInfo->height = cloneFrame.height() - cloneFrame.height()%8;
    newImage->vInfo->framerate = framerate_;

    cloneFrame.unmap();

    Q_ASSERT(newImage->data);

#ifdef _WIN32
   if (output_ == DT_RGB32VIDEO)
   {
     newImage->vInfo->flippedVertically = true;
     newImage = normalizeOrientation(std::move(newImage));
   }
#endif

    sendOutput(std::move(newImage));
  }
}
