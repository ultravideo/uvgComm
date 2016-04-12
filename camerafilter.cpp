#include "camerafilter.h"

#include "cameraframegrabber.h"

//#include <QCameraViewfinder>
#include <QCameraInfo>

#include <QtDebug>

CameraFilter::CameraFilter()
{
  camera_ = new QCamera(QCameraInfo::defaultCamera());
  cameraFrameGrabber_ = new CameraFrameGrabber();
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

  qDebug() << "Frame generated: " << pf;

  Q_ASSERT(pf == QVideoFrame::Format_RGB32);

  Data * newImage = new Data;

  newImage->type = RPG32VIDEO;
  std::unique_ptr<uchar> uu(new uchar[cloneFrame.mappedBytes()]);
  newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes()]);

  uchar *bits = cloneFrame.bits();

  memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes());
  newImage->data_size = cloneFrame.mappedBytes();
  newImage->width = cloneFrame.width();
  newImage->height = cloneFrame.height();

  std::unique_ptr<Data> u_newImage( newImage );

  cloneFrame.unmap();

  Q_ASSERT(u_newImage->data);

  putOutput(std::move(u_newImage));
}
