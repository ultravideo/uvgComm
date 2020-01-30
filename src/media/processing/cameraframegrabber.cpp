#include "cameraframegrabber.h"

#include "common.h"

#include <QDebug>

CameraFrameGrabber::CameraFrameGrabber(QObject *parent) :
  QAbstractVideoSurface(parent)
{}

bool CameraFrameGrabber::present(const QVideoFrame &frame)
{
  if (frame.isValid()) {
    emit frameAvailable(frame);
    return true;
  }
  printDebug(DEBUG_WARNING, this,  "Not a valid frame");
  return false;
}

QList<QVideoFrame::PixelFormat> CameraFrameGrabber::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
  Q_UNUSED(handleType);
  return QList<QVideoFrame::PixelFormat>()
      << QVideoFrame::Format_YUV420P
      << QVideoFrame::Format_YUYV
      << QVideoFrame::Format_NV12
      << QVideoFrame::Format_YV12
      << QVideoFrame::Format_RGB32;
}
