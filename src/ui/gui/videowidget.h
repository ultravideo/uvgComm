#pragma once

#include "videointerface.h"
#include "videodrawhelper.h"

#include <QPainter>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <memory>

class StatisticsInterface;

class VideoWidget : public QWidget, public VideoInterface
{
  Q_OBJECT
  Q_INTERFACES(VideoInterface)
public:
  VideoWidget(QWidget* parent = nullptr, uint32_t sessionID = 0,
              uint8_t borderSize = 1);
  ~VideoWidget();

  virtual void setStats(StatisticsInterface* stats)
  {
    stats_ = stats;
  }

  // Takes ownership of the image data
  virtual void inputImage(std::unique_ptr<uchar[]> data, QImage &image, int64_t timestamp);

  virtual void drawMicOffIcon(bool status);

  virtual void enableOverlay(int roiQP, int backgroundQP, int brushSize,
                             bool showGrid, bool pixelBased, QSize videoResolution);
  virtual void resetOverlay();

  virtual std::unique_ptr<int8_t[]> getRoiMask(int& width, int& height, int qp, bool scaleToInput);

  virtual VideoFormat supportedFormat()
  {
    return VIDEO_RGB32;
  }

  virtual bool isVisible()
  {
    return QWidget::isVisible();
  }

signals:

  void reattach(uint32_t sessionID);
  void detach(uint32_t sessionID);

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
};
