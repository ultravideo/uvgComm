#pragma once

#include <QCamera>

#include "filter.h"

class CameraFrameGrabber;

class CameraFilter : public Filter
{
  Q_OBJECT

public:
  CameraFilter();
  ~CameraFilter();

  virtual bool canHaveInputs() const
  {
    return false;
  }

  virtual bool canHaveOutputs() const
  {
    return true;
  }


private slots:
  void handleFrame(const QVideoFrame &frame);

protected:
  void run();

private:
  QCamera *camera;
  CameraFrameGrabber *cameraFrameGrabber_;
};
