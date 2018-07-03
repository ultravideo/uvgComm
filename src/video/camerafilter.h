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

  virtual bool init();

  virtual void start();
  virtual void stop();

private slots:
  // qcamera calls this when frame available
  void handleFrame(const QVideoFrame &frame);

protected:
  void process();

private:

  void printSupportedFormats();
  void printSupportedResolutions(QCameraViewfinderSettings &viewsettings);

  QCamera *camera_;
  CameraFrameGrabber *cameraFrameGrabber_;

  uint16_t framerate_;

  QMutex frameMutex_;
  std::deque<QVideoFrame> frames_;
};
