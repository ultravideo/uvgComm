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

class VideoYUVWidget : public QOpenGLWidget, public VideoInterface, protected QOpenGLFunctions
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

  virtual VideoFormat supportedFormat()
  {
    //return VIDEO_RGB32;
    return VIDEO_YUV420;
  }

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
  virtual void resizeGL(int width, int height);

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

  QOpenGLShaderProgram* prog_;

  QOpenGLBuffer vbo_;

  GLuint m_posAttr;
  GLuint m_colAttr;
  GLuint m_matrixUniform;
  int m_frame;
};
