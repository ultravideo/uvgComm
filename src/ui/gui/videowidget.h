#pragma once

#include "videointerface.h"
#include "videodrawhelper.h"

#include "global.h"

#include <QPainter>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>
#include <QTimer>

#include <memory>

class StatisticsInterface;

class VideoWidget : public QWidget, public VideoInterface
{
  Q_OBJECT
  //Q_INTERFACES(VideoInterface)
public:
  VideoWidget(QWidget* parent = nullptr, uint32_t sessionID = 0, LayoutID layoutID = 0,
              uint8_t borderSize = 1);
  ~VideoWidget();

  virtual void setStats(StatisticsInterface* stats)
  {
    stats_ = stats;
  }

  // Takes ownership of the image data
  virtual void inputImage(std::unique_ptr<uchar[]> data, QImage &image, int64_t timestamp);

#ifdef KVAZZUP_HAVE_ONNX_RUNTIME
  virtual void inputDetections(std::vector<Detection> detections, QSize original_size, int64_t timestamp);
#endif

  virtual void drawMicOffIcon(bool status);

  virtual void visualizeROIMap(RoiMap& map, int qp);

  virtual std::unique_ptr<int8_t[]> getRoiMask(int& width, int& height, int qp, bool scaleToInput);

  virtual VideoFormat supportedFormat()
  {
    return VIDEO_RGB32;
  }

  virtual bool isVisible()
  {
    return QWidget::isVisible();
  }

  void enableOverlay(int roiQP, int backgroundQP, int brushSize,
                     bool showGrid, bool pixelBased, QSize videoResolution);
  void disableOverlay();
  void resetOverlay();

signals:

  void reattach(LayoutID layoutID_);
  void detach(LayoutID layoutID_);

  void newImage();
protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mousePressEvent(QMouseEvent *e);
  void mouseReleaseEvent(QMouseEvent *e);
  void mouseMoveEvent(QMouseEvent *e);
  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void enterFullscreen();
  void exitFullscreen();

  QMutex drawMutex_;

  StatisticsInterface* stats_;
  uint32_t sessionID_;
  VideoDrawHelper helper_;

  QTimer updateTimer_;
};
