#include "camerafilter.h"

#include "cameraframegrabber.h"

//#include <QCameraViewfinder>
#include <QCameraInfo>

#include <QtDebug>

CameraFilter::CameraFilter(QSize resolution):camera_(),
  cameraFrameGrabber_(),
  resolution_(resolution)
{
  name_ = "CamF";
  camera_ = new QCamera(QCameraInfo::defaultCamera());
  cameraFrameGrabber_ = new CameraFrameGrabber();

  Q_ASSERT(camera_ && cameraFrameGrabber_);

  camera_->setViewfinder(cameraFrameGrabber_);

  connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(handleFrame(QVideoFrame)));

  camera_->start();
}

CameraFilter::~CameraFilter()
{
  delete camera_;
  delete cameraFrameGrabber_;
}

void CameraFilter::process()
{
  // do nothing because camera input is handled via camera signal
}


void CameraFilter::stop()
{
  camera_->stop();
  Filter::stop();
}

void CameraFilter::handleFrame(const QVideoFrame &frame)
{
  QVideoFrame cloneFrame(frame);
  cloneFrame.map(QAbstractVideoBuffer::ReadOnly);

  QVideoFrame::PixelFormat pf = cloneFrame.pixelFormat();

  Q_ASSERT(pf == QVideoFrame::Format_RGB32);

  Data * newImage = new Data;

  newImage->type = RGB32VIDEO;
  std::unique_ptr<uchar> uu(new uchar[cloneFrame.mappedBytes()]);
  newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes()]);

  uchar *bits = cloneFrame.bits();

  memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes());
  newImage->data_size = cloneFrame.mappedBytes();
  newImage->width = cloneFrame.width();
  newImage->height = cloneFrame.height();

  QImage image(
        newImage->data.get(),
        newImage->width,
        newImage->height,
        QImage::Format_RGB32);

  QImage image2 = image.scaled(resolution_);
  memcpy(newImage->data.get(), image2.bits(), image2.byteCount());
  newImage->width = resolution_.width();
  newImage->height = resolution_.height();
  newImage->data_size = image2.byteCount();
  //qDebug() << "Frame generated. Format: " << pf
  //         << " width: " << newImage->width << ", height: " << newImage->height;

  std::unique_ptr<Data> u_newImage( newImage );
  cloneFrame.unmap();

  Q_ASSERT(u_newImage->data);
  sendOutput(std::move(u_newImage));
}
