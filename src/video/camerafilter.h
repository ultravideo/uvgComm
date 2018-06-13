#pragma once
#include "filter.h"

#include <QCamera>
#include <QVideoFrame>

class CameraFrameGrabber;

class CameraFilter : public Filter
{
  Q_OBJECT

public:
  CameraFilter(QString id, StatisticsInterface* stats);
  ~CameraFilter();

  virtual void start();
  virtual void stop();

  void scaleFrame(const QVideoFrame &cloneFrame, Data *newImage, QSize resolution);

  void printSupportedFormats();
private slots:
  // qcamera calls this when frame available
  void handleFrame(const QVideoFrame &frame);

protected:
  void process();

private:
  QCamera *camera_;
  CameraFrameGrabber *cameraFrameGrabber_;

  uint16_t framerate_;

  QMutex frameMutex_;
  std::deque<QVideoFrame> frames_;
};
