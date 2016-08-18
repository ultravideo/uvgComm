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
  VideoWidget(QWidget* parent = 0);
  ~VideoWidget();
  void inputImage(std::unique_ptr<uchar[]> input,
                  QImage &image, QSize padding);

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);

private:
  bool hasImage_;
  QImage::Format imageFormat_;
  QRect targetRect_;

  QMutex drawMutex_;
  QImage currentImage_;
  std::unique_ptr<uchar[]> input_;
};
