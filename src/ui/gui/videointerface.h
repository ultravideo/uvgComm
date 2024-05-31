#pragma once

#include "global.h"

#include <QImage>

#include <memory>

#include "media/processing/detection_types.h"

class QWidget;
class StatisticsInterface;

// a very simple interface for drawing video to screen.
// an object using this interface can be created using VideoviewFactory
// The purpose of this interface and videoviewfactory is to enable
// a more flexible way of modifying drawing interface and its appearance.

enum VideoFormat {VIDEO_RGB32, VIDEO_YUV420};

class VideoInterface
{
public:

  // destructor needs to be declared for C++ interfaces
  virtual ~VideoInterface(){}

  // set stats to use with this video view.
  virtual void setStats(StatisticsInterface* stats) = 0;

  // Takes ownership of the image data
  virtual void inputImage(std::unique_ptr<uchar[]> data, QImage &image, int64_t timestamp) = 0;

#ifdef KVAZZUP_HAVE_ONNX_RUNTIME
  virtual void inputDetections(std::vector<Detection> detections, QSize original_size, int64_t timestamp) = 0;
#endif

  virtual void drawMicOffIcon(bool status) = 0;
  virtual std::unique_ptr<int8_t[]> getRoiMask(int& width, int& height, int qp, 
                                               bool scaleToInput) = 0;

  virtual VideoFormat supportedFormat() = 0;

signals:
  virtual void reattach(LayoutID layoutID) = 0;
  virtual void detach(LayoutID layoutID) = 0;
  virtual bool isVisible() = 0;
};

//Q_DECLARE_INTERFACE(VideoInterface, "VideoInterface")
