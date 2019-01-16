#pragma once

class QWidget;
class QMouseEvent;
class QKeyEvent;

#include <QObject>
#include <QSize>
#include <QImage>
#include <QRect>

#include <deque>
#include <memory>

/*
 * Purpose of the VideoDrawHelper is to process all the mouse and keyboard events
 * for video view widgets. It also includes the buffer for images waiting to be drawn.
*/


class VideoDrawHelper : public QObject
{
  Q_OBJECT
public:
  VideoDrawHelper(uint32_t sessionID, uint8_t borderSize);
  ~VideoDrawHelper();

  void initWidget(QWidget* widget);

  bool readyToDraw();
  void inputImage(QWidget *widget, std::unique_ptr<uchar[]> data, QImage &image);

  // returns whether this is a new image or the previous one
  bool getRecentImage(QImage& image);

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

signals:

  void reattach(uint32_t sessionID_, QWidget* view);
  void deattach(uint32_t sessionID_);

private:
  void enterFullscreen(QWidget* widget);
  void exitFullscreen(QWidget* widget);



  uint32_t sessionID_;

  QWidget* tmpParent_;

  QRect targetRect_;
  QRect newFrameRect_;

  bool firstImageReceived_;
  QSize previousSize_;
  QImage lastImage_;
  std::unique_ptr<uchar[]> lastImageData_;

  int borderSize_;

  std::deque<QImage> viewBuffer_;
  std::deque<std::unique_ptr<uchar[]>> dataBuffer_;
};
