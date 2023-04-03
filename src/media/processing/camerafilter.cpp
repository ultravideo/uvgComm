#include "camerafilter.h"

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

    QObject::connect(&sink_, &QVideoSink::videoFrameChanged,
                     this, &CameraFilter::handleFrame);

    capture_.setVideoSink(&sink_);
    capture_.setCamera(camera_);

    currentInputFormat_ = settings.value(SettingsKey::videoInputFormat).toString();
    currentResolutionID_ = settings.value(SettingsKey::videoResolutionID).toInt();

    currentFramerateID_ = settings.value(SettingsKey::videoFramerateID).toInt();
    framerateNumerator_ = settings.value(SettingsKey::videoFramerateNumerator).toInt();
    framerateDenominator_ = settings.value(SettingsKey::videoFramerateDenominator).toInt();

    QVideoFrameFormat::PixelFormat format = convertFormat(currentInputFormat_);
    QList<QCameraFormat> options = camera_->cameraDevice().videoFormats();

    QList<QSize> supportedResolutions;
    QList<int> framerates;

    QCameraFormat selectedOption;

    bool foundOption = false;
    for (auto& formatOption : options)
    {

      if (formatOption.pixelFormat() == format)
      {
        if (!supportedResolutions.contains(formatOption.resolution()))
        {
            supportedResolutions.push_back(formatOption.resolution());
        }

        if (supportedResolutions.size() - 1 == currentResolutionID_)
        {
          framerates.push_back(formatOption.maxFrameRate());

          if (framerates.size() - 1 == currentFramerateID_)
          {
              Logger::getLogger()->printNormal(this, "Found camera option");
              selectedOption = formatOption;
              foundOption = true;
          }
        }
      }
    }

    if (!options.empty())
    {
      if (!foundOption)
      {
        camera_->setCameraFormat(options.first());
      }
      else
      {
        camera_->setCameraFormat(selectedOption);
      }
    }

    camera_->start();
    Logger::getLogger()->printImportant(this, "Starting camera.");
  }
  else
  {
    return false;
  }
  return true;
}

QVideoFrameFormat::PixelFormat CameraFilter::convertFormat(QString formatString)
{
  if (currentInputFormat_ == "RGB24")
  {
    output_ = DT_RGB32VIDEO;
    return QVideoFrameFormat::Format_RGBX8888;
  }
  else if (currentInputFormat_ == "RGB32")
  {
    output_ = DT_RGB32VIDEO;
    return QVideoFrameFormat::Format_RGBA8888;
  }
  if(currentInputFormat_ == "MJPG")
  {
    output_ = DT_RGB32VIDEO;
    return QVideoFrameFormat::Format_Jpeg;
  }
  else if(currentInputFormat_ == "YUV420P")
  {
    output_ = DT_YUV420VIDEO;
    return QVideoFrameFormat::Format_YUV420P;
  }
  else if(currentInputFormat_ == "YUYV")
  {
    output_ = DT_YUYVVIDEO;
    return QVideoFrameFormat::Format_YUYV;
  }

  Logger::getLogger()->printError(this, "Input format not supported",
                                  {"Format"}, {currentInputFormat_});
  output_ = DT_NONE;

  return QVideoFrameFormat::Format_Invalid;
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
  if (frame.planeCount() == 1)
  {
    frames_.push_back(frame);
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Video frame has more than one plane");
  }
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

    if (frame.pixelFormat() == QVideoFrameFormat::Format_Jpeg)
    {
      Logger::getLogger()->printWarning(this, "Unsupported frame format");
      return;
    }


    // capture the frame data
    std::unique_ptr<Data> newImage = initializeData(output_, DS_LOCAL);
    newImage->presentationTime = QDateTime::currentMSecsSinceEpoch();
    newImage->type = output_;

    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QVideoFrame::ReadOnly);

    int plane = 0;
    newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes(plane)]);
    uchar *bits = cloneFrame.bits(plane);

    memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes(plane));
    newImage->data_size = cloneFrame.mappedBytes(plane);

    // kvazaar requires divisable by 8 resolution
    newImage->vInfo->width = cloneFrame.width() - cloneFrame.width()%8;
    newImage->vInfo->height = cloneFrame.height() - cloneFrame.height()%8;
    newImage->vInfo->framerateNumerator = framerateNumerator_;
    newImage->vInfo->framerateDenominator = framerateDenominator_;

/*
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
