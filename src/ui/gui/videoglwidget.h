#pragma once

#include "videointerface.h"
#include "videodrawhelper.h"

#include <QPainter>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <QtOpenGL>

#include <deque>
#include <memory>


// TODO: duplicate code in this and videowidget.

class StatisticsInterface;

class VideoGLWidget : public QOpenGLWidget, public VideoInterface
{
  Q_OBJECT
  Q_INTERFACES(VideoInterface)
public:
  VideoGLWidget(QWidget* parent = nullptr, uint32_t sessionID = 0,
                uint32_t index = 0, uint8_t borderSize = 1);
  ~VideoGLWidget();

  void setStats(StatisticsInterface* stats)
  {
    stats_ = stats;
  }

  // Takes ownership of the image data
  void inputImage(std::unique_ptr<uchar[]> data, QImage &image, int64_t timestamp);

  virtual std::unique_ptr<int8_t[]> getRoiMask(int& width, int& height, int qp, bool scaleToInput);

  virtual void drawMicOffIcon(bool status);

  virtual void enableOverlay(int roiQP, int backgroundQP, int brushSize, 
                             bool showGrid, bool pixelBased);
  virtual void resetOverlay();

  virtual VideoFormat supportedFormat()
  {
    return VIDEO_RGB32;
  }

  virtual bool isVisible()
  {
    return QWidget::isVisible();
  }

  static unsigned int number_;

signals:

  // for reattaching after fullscreenmode
  void reattach(uint32_t sessionID_);
  void detach(uint32_t sessionID_, uint32_t index, QWidget* widget);

  void newImage();
protected:

  // QOpenGLwidget events
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  QMutex drawMutex_;

  StatisticsInterface* stats_;
  uint32_t sessionID_;

  VideoDrawHelper helper_;
};
