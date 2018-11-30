#pragma once

#include "gui/videointerface.h"

#include <QOpenGLFunctions_2_0>

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

// The normal QOpenGLFunctions provides access to ES API so the whole API is used instead

class VideoYUVWidget : public QOpenGLWidget, public VideoInterface, protected QOpenGLFunctions_2_0
{
  Q_OBJECT
  Q_INTERFACES(VideoInterface)
public:
  VideoYUVWidget(QWidget* parent = nullptr, uint32_t sessionID = 0, uint8_t borderSize = 1);
  ~VideoYUVWidget();

  void setStats(StatisticsInterface* stats)
  {
    stats_ = stats;
  }

  // Takes ownership of the image data
  void inputImage(std::unique_ptr<uchar[]> data, QImage &image);

  static unsigned int number_;

signals:

  // for reattaching after fullscreenmode
  void reattach(uint32_t sessionID_, QWidget* view);

  void newImage();
protected:

  // QOpenGLwidget events
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

  virtual void initializeGL();
  virtual void paintGL();
  //virtual void resizeGL();

private:

  // update the rect in case the window or input has changed.
  void updateTargetRect();

  void enterFullscreen();
  void exitFullscreen();

  void drawOpenGL(bool updateImage);

  bool firstImageReceived_;

  QRect targetRect_;

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

  GLuint texture_;
};
