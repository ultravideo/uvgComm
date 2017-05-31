#pragma once
#include <QPainter>
#include <QWidget>
#include <QRect>
#include <QSize>
#include <QImage>
#include <QMutex>

#include <memory>

class VideoWidget : public QWidget
{
  Q_OBJECT
public:
  VideoWidget(QWidget* parent = NULL);
  ~VideoWidget();
  void inputImage(std::unique_ptr<uchar[]> input,
                  QImage &image);

  static unsigned int number_;

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);

private:

  void updateTargetRect();

  bool hasImage_;
  QImage::Format imageFormat_;
  QRect targetRect_;

  QMutex drawMutex_;
  QImage currentImage_;
  std::unique_ptr<uchar[]> input_;

  bool updated_;

  unsigned int id_;
};
