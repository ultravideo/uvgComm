#pragma once

#include "filter.h"

#include <QCamera>
#include <QVideoFrame>

class CameraFrameGrabber;

class CameraFilter : public Filter
{
  Q_OBJECT

public:
  CameraFilter(StatisticsInterface* stats, QSize resolution);
  ~CameraFilter();

  virtual bool isInputFilter() const
  {
    return true;
  }

  virtual bool isOutputFilter() const
  {
    return false;
  }

  virtual void stop();

private slots:
  // qcamera calls this when frame available
  void handleFrame(const QVideoFrame &frame);

protected:
  void process();

private:
  QCamera *camera_;
  CameraFrameGrabber *cameraFrameGrabber_;

  QSize resolution_;
};
