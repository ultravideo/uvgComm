#pragma once
#include <QPainter>
#include <QFrame>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <deque>
#include <memory>

class VideoWidget : public QFrame
{
  Q_OBJECT
public:
  VideoWidget(QWidget* parent = NULL, uint8_t borderSize = 1);
  ~VideoWidget();

  // Takes ownership of the image data
  void inputImage(std::unique_ptr<uchar[]> data, QImage &image);

  static unsigned int number_;

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

private:

  void updateTargetRect();

  bool firstImageReceived_;

  QRect targetRect_;
  QRect newFrameRect_;

  QMutex drawMutex_;
  QSize previousSize_;
  std::deque<QImage> viewBuffer_;
  std::deque<std::unique_ptr<uchar[]>> dataBuffer_;
  QImage lastImage_;
  std::unique_ptr<uchar[]> lastImageData_;


  unsigned int id_;

  unsigned int borderSize_;
};
