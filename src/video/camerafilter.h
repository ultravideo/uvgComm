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

  // setup camera device
  virtual bool init();

  // start/stop camera device and this filter.
  virtual void start();
  virtual void stop();

private slots:
  // qcamera calls this when frame available
  // copies the frame to buffer for further processing
  void handleFrame(const QVideoFrame &frame);

protected:

  // this sends the video frame forward instead of handle frame, because
  // then we get the processing to a separate non GUI thread which
  // might be a problem with high resolutions.
  void process();

private:

  // debug print functions.
  void printSupportedFormats();
  void printSupportedResolutions(QCameraViewfinderSettings &viewsettings);

  QCamera *camera_;
  CameraFrameGrabber *cameraFrameGrabber_;

  uint16_t framerate_;

  QMutex frameMutex_;
  std::deque<QVideoFrame> frames_;
};
