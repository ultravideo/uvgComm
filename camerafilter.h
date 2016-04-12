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
  void handleFrame(const QVideoFrame &frame);

protected:
  void process();

private:
  QCamera *camera_;
  CameraFrameGrabber *cameraFrameGrabber_;
};
