#pragma once
#include <QPainter>
#include <QFrame>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <memory>

class VideoWidget : public QFrame
{
  Q_OBJECT
public:
  VideoWidget(QWidget* parent = NULL, uint8_t borderSize = 1);
  ~VideoWidget();
  void inputImage(std::unique_ptr<uchar[]> input,
                  QImage &image);

  static unsigned int number_;

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

private:

  void updateTargetRect();

  bool hasImage_;
  QImage::Format imageFormat_;
  QRect targetRect_;
  QRect newFrameRect_;

  QMutex drawMutex_;
  QSize previousSize_;
  QImage currentImage_;
  std::unique_ptr<uchar[]> input_;

  bool updated_;

  unsigned int id_;

  unsigned int borderSize_;
};
