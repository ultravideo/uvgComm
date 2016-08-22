#include "videowidget.h"

#include <QPaintEvent>
#include <QDebug>
#include <QCoreApplication>

unsigned int VideoWidget::number_ = 0;

VideoWidget::VideoWidget(QWidget* parent): QWidget(parent),
  hasImage_(false),
  id_(number_)
{
  ++number_;
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground, true);

  QPalette palette = this->palette();
  palette.setColor(QPalette::Background, Qt::black);
  setPalette(palette);

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

VideoWidget::~VideoWidget()
{}

void VideoWidget::inputImage(std::unique_ptr<uchar[]> input,
                             QImage &image)
{
  qDebug() << "Inputting image:" << id_;
  Q_ASSERT(input);
  drawMutex_.lock();
  input_ = std::move(input);
  currentImage_ = image;
  hasImage_ = true;
  drawMutex_.unlock();

  updateTargetRect();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  qDebug() << "PaintEvent for widget:" << id_;
  QPainter painter(this);

  if(hasImage_)
  {
    drawMutex_.lock();
    Q_ASSERT(input_);

    painter.drawImage(targetRect_, currentImage_);
    drawMutex_.unlock();
  }
  else
  {
    painter.fillRect(event->rect(), QBrush(QColor(0,0,0)));
  }
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
  qDebug() << "ResizeEvent:" << id_;
  QWidget::resizeEvent(event);
  updateTargetRect();
}

void VideoWidget::updateTargetRect()
{
  if(hasImage_)
  {
    QSize size = currentImage_.size();
    size.scale(QWidget::size().boundedTo(size), Qt::KeepAspectRatio);

    targetRect_ = QRect(QPoint(0, 0), size);
    targetRect_.moveCenter(rect().center());
  }
}
