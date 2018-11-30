#pragma once

#include "gui/videointerface.h"

#include <QPainter>
#include <QFrame>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <deque>
#include <memory>

class StatisticsInterface;

class VideoWidget : public QFrame, public VideoInterface
{
  Q_OBJECT
  Q_INTERFACES(VideoInterface)
public:
  VideoWidget(QWidget* parent = nullptr, uint32_t sessionID = 0, uint8_t borderSize = 1);
  ~VideoWidget();

  virtual void setStats(StatisticsInterface* stats)
  {
    stats_ = stats;
  }

  // Takes ownership of the image data
  virtual void inputImage(std::unique_ptr<uchar[]> data, QImage &image);

  virtual VideoFormat supportedFormat()
  {
    return VIDEO_RGB32;
  }

signals:

  void reattach(uint32_t sessionID_, QWidget* view);

  void newImage();
protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void updateTargetRect();

  void enterFullscreen();
  void exitFullscreen();

  bool firstImageReceived_;

  QRect targetRect_;
  QRect newFrameRect_;

  QMutex drawMutex_;
  QSize previousSize_;
  std::deque<QImage> viewBuffer_;
  std::deque<std::unique_ptr<uchar[]>> dataBuffer_;
  QImage lastImage_;
  std::unique_ptr<uchar[]> lastImageData_;

  StatisticsInterface* stats_;

  uint32_t sessionID_;

  unsigned int borderSize_;

  QWidget* tmpParent_;
  QLayout* ourLayout_;
};
