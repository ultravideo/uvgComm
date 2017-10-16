#include "cameraframegrabber.h"

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
  qWarning() << "Not a valid frame";
  return false;
}

QList<QVideoFrame::PixelFormat> CameraFrameGrabber::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
  Q_UNUSED(handleType);
  // TODO: Implement raw YUV from camera.
  return QList<QVideoFrame::PixelFormat>()
      << QVideoFrame::Format_RGB32;
}
