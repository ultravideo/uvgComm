#pragma once

#include "videointerface.h"
#include "videodrawhelper.h"

#include <QPainter>
#include <QFrame>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <memory>

class StatisticsInterface;

class VideoWidget : public QFrame, public VideoInterface
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

  virtual VideoFormat supportedFormat()
  {
    return VIDEO_RGB32;
  }

signals:

  void reattach(uint32_t sessionID);
  void detach(uint32_t sessionID);

  void newImage();
protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void enterFullscreen();
  void exitFullscreen();

  QMutex drawMutex_;

  StatisticsInterface* stats_;
  uint32_t sessionID_;
  uint32_t index_;

  VideoDrawHelper helper_;
};
