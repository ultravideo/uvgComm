#pragma once

#include <QObject>
#include <QSize>
#include <QImage>
#include <QRect>
#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>

#include <QElapsedTimer>
#include <QMutex>

#include <deque>
#include <memory>

/*
 * Purpose of the VideoDrawHelper is to process all the mouse and keyboard events
 * for video view widgets. It also includes the buffer for images waiting to be drawn.
*/

// This class could possibly be combined with displayfilter.

// TODO: When the fullscreen mode is activated and there is another program on the background,
// that another program can be on top of the fullscreen for some reason.

class VideoDrawHelper : public QObject
{
  Q_OBJECT
public:
  VideoDrawHelper(uint32_t sessionID, uint32_t index, uint8_t borderSize);
  ~VideoDrawHelper();

  void initWidget(QWidget* widget);

  void enableOverlay();

  bool readyToDraw();
  void inputImage(QWidget *widget, std::unique_ptr<uchar[]> data, QImage &image, int64_t timestamp);

  // returns whether this is a new image or the previous one
  bool getRecentImage(QImage& image);

  void drawOverlay(QPainter& painter);
  void addPointToOverlay(const QPointF& position, bool addPoint, bool removePoint);

  void mouseDoubleClickEvent(QWidget* widget);
  void keyPressEvent(QWidget* widget, QKeyEvent* event);

  // update the rect in case the window or input has changed.
  void updateTargetRect(QWidget* widget);

  QRect getTargetRect()
  {
    return targetRect_;
  }

  QRect getFrameRect()
  {
    return newFrameRect_;
  }

  std::shared_ptr<int8_t[]> getRoiMask(int width, int height);

signals:

  void reattach(uint32_t sessionID_);
  void detach(uint32_t sessionID_, uint32_t index, QWidget* widget);

private:
  void enterFullscreen(QWidget* widget);
  void exitFullscreen(QWidget* widget);

  void createROIMask(int width, int height);

  uint32_t sessionID_;
  uint32_t index_;

  QWidget* tmpParent_;

  QRect targetRect_;
  QRect newFrameRect_;

  bool firstImageReceived_;
  QSize previousSize_;

  int borderSize_;

  struct Frame
  {
    QImage image;
    std::unique_ptr<uchar[]> data;
    int64_t timestamp;
  };

  Frame lastFrame_;
  std::deque<Frame> frameBuffer_;

  int64_t currentFrame_;

  QMutex roiMutex_;
  std::shared_ptr<int8_t[]> roiMask_;
  int roiSize_;

  bool drawOverlay_;
  QImage overlay_;
};
