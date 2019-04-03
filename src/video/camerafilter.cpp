#include "camerafilter.h"

#include "cameraframegrabber.h"
#include "statisticsinterface.h"

#include "common.h"

#include <QSettings>
#include <QCameraInfo>
#include <QTime>
#include <QtDebug>


CameraFilter::CameraFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Camera", stats, NONE, RGB32VIDEO),
  camera_(),
  cameraFrameGrabber_(),
  framerate_(0)
{}

CameraFilter::~CameraFilter()
{
  delete camera_;
  delete cameraFrameGrabber_;
}

bool CameraFilter::init()
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

  if(cameras.size() == 0)
  {
    qDebug() << "No camera found!";
    return false;
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  QString deviceName = settings.value("video/Device").toString();
  int deviceID = settings.value("video/DeviceID").toInt();

  // if the deviceID has changed
  if (deviceID < cameras.size() && cameras[deviceID].description() != deviceName)
  {
    // search for device with same name
    for(int i = 0; i < cameras.size(); ++i)
    {
      if(cameras.at(i).description() == deviceName)
      {
        qDebug() << "Found camera with name:" << cameras.at(i).description() << "and id:" << i;
        deviceID = i;
        break;
      }
    }
    // previous camera could not be found, use first.
    qDebug() << "Did not find camera name:" << deviceName << " Using first";
    deviceID = 0;
  }

  qDebug() << "Iniating Qt camera with device ID:" << deviceID << " cameras:" << cameras.size();

  camera_ = new QCamera(cameras.at(deviceID));
  cameraFrameGrabber_ = new CameraFrameGrabber();

#ifndef __linux__
  camera_->load();
  while(camera_->state() != QCamera::LoadedState)
  {
    qSleep(5);
  }
#endif

  Q_ASSERT(camera_ && cameraFrameGrabber_);
  camera_->setViewfinder(cameraFrameGrabber_);

  connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(handleFrame(QVideoFrame)));

  QCameraViewfinderSettings viewSettings = camera_->viewfinderSettings();
  printSupportedFormats();

  // TODO: this should be a temprary hack until dshow is replaced by qcamera
  if(settings.value("video/InputFormat").toString() == "MJPG")
  {
    viewSettings.setPixelFormat(QVideoFrame::Format_Jpeg);
    output_ = RGB32VIDEO;
  }
  else if(settings.value("video/InputFormat").toString() == "YUY2")
  {
    viewSettings.setPixelFormat(QVideoFrame::Format_YUYV);
    output_ = RGB32VIDEO;
  }
  else if(settings.value("video/InputFormat").toString() == "NV12")
  {
    viewSettings.setPixelFormat(QVideoFrame::Format_NV12);
    output_ = RGB32VIDEO;
  }
  else if(settings.value("video/InputFormat").toString() == "I420")
  {
    viewSettings.setPixelFormat(QVideoFrame::Format_YUV420P);
    output_ = RGB32VIDEO;
  }
  else if(settings.value("video/InputFormat").toString() == "YV12")
  {
    viewSettings.setPixelFormat(QVideoFrame::Format_YV12);
    output_ = RGB32VIDEO;
  }

  //printSupportedResolutions(viewSettings);

  QList<QSize> resolutions = camera_->supportedViewfinderResolutions(viewSettings);

  int resolutionID = settings.value("video/ResolutionID").toInt();
  if(resolutions.size() >= resolutionID && !resolutions.empty())
  {
    QSize resolution = resolutions.at(resolutionID);
    viewSettings.setResolution(QSize(resolution.width(),
                                     resolution.height()));
  }
  else {
    viewSettings.setResolution(QSize(0,0));
  }

  QList<QCamera::FrameRateRange> framerates = camera_->supportedViewfinderFrameRateRanges(viewSettings);

  int framerateID = settings.value("video/FramerateID").toInt();

#ifndef __linux__
  if (!framerates.empty())
  {
    if(framerateID < framerates.size())
    {
      viewSettings.setMaximumFrameRate(framerates.at(framerateID).maximumFrameRate);
      viewSettings.setMinimumFrameRate(framerates.at(framerateID).minimumFrameRate);
    }
    else {
      viewSettings.setMaximumFrameRate(framerates.back().maximumFrameRate);
      viewSettings.setMinimumFrameRate(framerates.back().minimumFrameRate);
    }
  }

#else
  viewSettings.setMaximumFrameRate(30);
  viewSettings.setMinimumFrameRate(30);
  viewSettings.setResolution(QSize(640, 480));
  viewSettings.setPixelFormat(QVideoFrame::Format_BGRA32);
  output_ = RGB32VIDEO;
#endif

  qDebug() << "Iniating, CameraFilter: Using following QCamera settings:";
  qDebug() << "Iniating, CameraFilter:---------------------------------------";
  qDebug() << "Iniating, CameraFilter: Format:" << viewSettings.pixelFormat();
  qDebug() << "Iniating, CameraFilter: Resolution:" << viewSettings.resolution();
  qDebug() << "Iniating, CameraFilter: FrameRate:" << viewSettings.minimumFrameRate() << "to" << viewSettings.maximumFrameRate();
  qDebug() << "Iniating, CameraFilter: ---------------------------------------";
  camera_->setViewfinderSettings(viewSettings);
  camera_->start();

  return Filter::init();
}


void CameraFilter::start()
{
  qDebug() << "Iniating, CameraFilter: Starting QCamera";
  if(camera_->state() == QCamera::LoadedState)
  {
    camera_->start();
  }
  Filter::start();
}

void CameraFilter::stop()
{
  if(camera_->state() == QCamera::ActiveState)
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
    qDebug() << "No frame";
    return;
  }
  QVideoFrame frame = frames_.front();
  frameMutex_.lock();
  frames_.pop_front();
  frameMutex_.unlock();

  while(true)
  {
    // do nothing because camera input is handled via camera signal
    if(framerate_ == 0)
    {
      QCameraViewfinderSettings settings = camera_->viewfinderSettings();

      getStats()->videoInfo(settings.maximumFrameRate(), settings.resolution());
      framerate_ = settings.maximumFrameRate();
    }

    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QAbstractVideoBuffer::ReadOnly);

    //QVideoFrame::PixelFormat pf = cloneFrame.pixelFormat();

    // capture the frame data
    Data * newImage = new Data;

    // set time
    timeval present_time;

    present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
    present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;

    newImage->presentationTime = present_time;
    newImage->type = output_;
    newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes()]);

    uchar *bits = cloneFrame.bits();

    memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes());
    newImage->data_size = cloneFrame.mappedBytes();
    // kvazaar requires divisable by 8 resolution
    newImage->width = cloneFrame.width() - cloneFrame.width()%8;
    newImage->height = cloneFrame.height() - cloneFrame.height()%8;
    newImage->source = LOCAL;
    newImage->framerate = framerate_;

    //qDebug() << "Frame generated. Format: " << pf
    //         << " width: " << newImage->width << ", height: " << newImage->height;

    std::unique_ptr<Data> u_newImage( newImage );
    cloneFrame.unmap();

    Q_ASSERT(u_newImage->data);
    sendOutput(std::move(u_newImage));

    if(frames_.empty())
    {
      return;
    }

    frame = frames_.front();
    frameMutex_.lock();
    frames_.pop_front();
    frameMutex_.unlock();
  }
}

void CameraFilter::printSupportedFormats()
{
  QList<QVideoFrame::PixelFormat> formats = camera_->supportedViewfinderPixelFormats();
  qDebug() << "Iniating, CameraFilter: Found" << formats.size() << "supported QCamera formats.";
  for(auto format : formats)
  {
    qDebug() << "Iniating, Camerafilter: QCamera supported format:" << format;
  }
}

void CameraFilter::printSupportedResolutions(QCameraViewfinderSettings& viewsettings)
{
  QList<QSize> resolutions = camera_->supportedViewfinderResolutions(viewsettings);
  qDebug() << "Iniating, CameraFilter: Found" << resolutions.size() << "supported QCamera resolutions.";
  for(auto reso : resolutions)
  {
    qDebug() << "Iniating, CameraFilter: QCamera supported resolutions:" << reso;
  }
}
