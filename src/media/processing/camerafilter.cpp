#include "camerafilter.h"

#include "cameraformats.h"
#include "statisticsinterface.h"

#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
#include <QTime>
#include <QMediaDevices>
#include <QVideoSink>

#include <chrono>
#include <thread>


CameraFilter::CameraFilter(QString id, StatisticsInterface *stats,
                           std::shared_ptr<ResourceAllocator> hwResources):
  Filter(id, "Camera", stats, hwResources, DT_NONE, DT_RGB32VIDEO),
  camera_(nullptr),
  framerateNumerator_(0),
  framerateDenominator_(0),
  resolutionWidth_(0),
  resolutionHeight_(0),
  currentDeviceName_(""),
  currentDeviceID_(-1),
  currentInputFormat_("")
{}


CameraFilter::~CameraFilter()
{
  if (camera_ != nullptr)
  {
    delete camera_;
  }
}


void CameraFilter::updateSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);

  QString deviceName = settings.value(SettingsKey::videoDevice).toString();
  int deviceID = settings.value(SettingsKey::videoDeviceID).toInt();
  QString inputFormat = settings.value(SettingsKey::videoInputFormat).toString();
  int resolutionWidth = settings.value(SettingsKey::videoResolutionWidth).toInt();
  int resolutionHeight = settings.value(SettingsKey::videoResolutionHeight).toInt();

  int32_t framerateNumerator = settings.value(SettingsKey::videoFramerateNumerator).toInt();
  int32_t framerateDenominator = settings.value(SettingsKey::videoFramerateDenominator).toInt();

  // update camera only if something has changed
  if (deviceName            != currentDeviceName_   ||
      deviceID              != currentDeviceID_     ||
      inputFormat           != currentInputFormat_  ||
      resolutionWidth       != resolutionWidth_     ||
      resolutionHeight      != resolutionHeight_    ||
      framerateNumerator    != framerateNumerator_  ||
      framerateDenominator  != framerateDenominator_)
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

  capture_.reset();
  sink_.reset();
}


bool CameraFilter::init()
{
  capture_ = std::unique_ptr<QMediaCaptureSession>(new QMediaCaptureSession);
  sink_ = std::unique_ptr<QVideoSink>(new QVideoSink);
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
  const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();

  if(cameras.size() == 0)
  {
    Logger::getLogger()->printWarning(this, "No camera found!");
    return false;
  }

  QSettings settings(settingsFile, settingsFileFormat);
  currentDeviceName_ = settings.value(SettingsKey::videoDevice).toString();
  currentDeviceID_ = settings.value(SettingsKey::videoDeviceID).toInt();

  if (settings.value(SettingsKey::videoInputFormat).toString() == "")
  {
    Logger::getLogger()->printWarning(this, "Camera format not set");
    return false;
  }

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

  if (currentDeviceID_ != -1 && currentDeviceID_ < cameras.size())
  {
    camera_ = new QCamera(cameras.at(currentDeviceID_));

    return true;
  }

  return false;
}


bool CameraFilter::cameraSetup()
{
  Q_ASSERT(camera_);

  if (camera_)
  {
    QSettings settings(settingsFile, settingsFileFormat);

    int waitRoundMS = 5;
    int totalWaitTime = 1000;
    int currentWaitTime = 0;
    while(!camera_->isAvailable())
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(waitRoundMS));
      currentWaitTime += waitRoundMS;
      if (currentWaitTime >= totalWaitTime)
      {
        Logger::getLogger()->printWarning(this, "Loading camera did not succeed.");
        return false;
      }
    }

    QObject::connect(sink_.get(), &QVideoSink::videoFrameChanged,
                     this, &CameraFilter::handleFrame);

    capture_->setVideoSink(sink_.get());
    capture_->setCamera(camera_);

    currentInputFormat_ = settings.value(SettingsKey::videoInputFormat).toString();
    resolutionWidth_ = settings.value(SettingsKey::videoResolutionWidth).toInt();
    resolutionHeight_ = settings.value(SettingsKey::videoResolutionHeight).toInt();
    framerateNumerator_ = settings.value(SettingsKey::videoFramerateNumerator).toInt();
    framerateDenominator_ = settings.value(SettingsKey::videoFramerateDenominator).toInt();
    output_ = stringToDatatype(currentInputFormat_);

    float framerate = (float)framerateNumerator_/framerateDenominator_;
    QVideoFrameFormat::PixelFormat format = stringToPixelFormat(currentInputFormat_);
    QList<QCameraFormat> options = camera_->cameraDevice().videoFormats();

    QCameraFormat selectedOption;

    bool foundOption = false;
    for (auto& formatOption : options)
    {
      if (formatOption.pixelFormat() == format &&
          formatOption.resolution().width() == resolutionWidth_ &&
          formatOption.resolution().height() == resolutionHeight_ &&
          formatOption.maxFrameRate() == framerate)
      {
        selectedOption = formatOption;
        foundOption = true;
      }
    }

    if (foundOption)
    {
      Logger::getLogger()->printNormal(this, "Found camera option");
      camera_->setCameraFormat(selectedOption);
    }
    else
    {
      Logger::getLogger()->printError(this, "Did not find camera option");
      return false;
    }
    if (!camera_->isActive())
    {
      camera_->start();
    }
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
    QVideoFrame frame = frames_.front(); // TODO: Crash here if call is started before camera has initialized in debugger
    frameMutex_.lock();
    frames_.pop_front();
    frameMutex_.unlock();

    // capture the frame data
    std::unique_ptr<Data> newImage = initializeData(output_, DS_LOCAL);
    newImage->creationTimestamp = QDateTime::currentMSecsSinceEpoch();
    newImage->presentationTimestamp = newImage->creationTimestamp;

    newImage->type = output_;

    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QVideoFrame::ReadOnly);

    size_t totalSize = 0;

    for (int plane = 0; plane < cloneFrame.planeCount(); ++plane)
    {
      totalSize += cloneFrame.mappedBytes(plane);
    }

    newImage->data = std::unique_ptr<uchar[]>(new uchar[totalSize]);

    uint8_t* ptr = newImage->data.get();
    for (int plane = 0; plane < cloneFrame.planeCount(); ++plane)
    {
      uchar *bits = cloneFrame.bits(plane);
      memcpy(ptr, bits, cloneFrame.mappedBytes(plane));
      ptr += cloneFrame.mappedBytes(plane);
    }

    newImage->data_size = totalSize;

    // kvazaar requires divisable by 8 resolution
    newImage->vInfo->width = cloneFrame.width() - cloneFrame.width()%8;
    newImage->vInfo->height = cloneFrame.height() - cloneFrame.height()%8;
    newImage->vInfo->framerateNumerator = framerateNumerator_;
    newImage->vInfo->framerateDenominator = framerateDenominator_;

/*
    printDataBytes("Camera bytes", newImage->data.get(), newImage->data_size, 3, 0);
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Captured frame",
                                    {"Width", "Height", "data size"},
                                    {QString::number(newImage->vInfo->width),
                                     QString::number(newImage->vInfo->height),
                                     QString::number(newImage->data_size)});

*/
    cloneFrame.unmap();

    Q_ASSERT(newImage->data);

#ifdef _WIN32
   if (output_ == DT_RGB32VIDEO)
   {
     newImage->vInfo->flippedVertically = true;

     // This flipping is done in self view and rgb32 to yuv filter
     //newImage = normalizeOrientation(std::move(newImage));
   }
#endif

    sendOutput(std::move(newImage));
  }
}
