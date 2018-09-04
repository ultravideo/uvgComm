#pragma once

#include <QAbstractVideoSurface>
#include <QList>

// This class is used by CameraFilter to capture frames from camera.

class CameraFrameGrabber : public QAbstractVideoSurface
{
  Q_OBJECT
public:
  explicit CameraFrameGrabber(QObject *parent = nullptr);

  QList<QVideoFrame::PixelFormat>
  supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const;

  bool present(const QVideoFrame &frame);

signals:
  void frameAvailable(const QVideoFrame &frame);
};
