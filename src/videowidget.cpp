#include "videowidget.h"

#include <QPaintEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>

unsigned int VideoWidget::number_ = 0;

VideoWidget::VideoWidget(QWidget* parent): QWidget(parent),
  hasImage_(false),
  updated_(false),
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
  Q_ASSERT(input);
  drawMutex_.lock();
  input_ = std::move(input);
  currentImage_ = image;
  hasImage_ = true;
  drawMutex_.unlock();
  updated_ = true;

  updateTargetRect();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  //qDebug() << "PaintEvent for widget:" << id_;
  QPainter painter(this);

  if(hasImage_)
  {
    if(updated_)
    {
      drawMutex_.lock();
      Q_ASSERT(input_);

      painter.drawImage(targetRect_, currentImage_);
      drawMutex_.unlock();
    }
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

void VideoWidget::keyPressEvent(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    qDebug() << "Esc key pressed";
    this->showNormal();
  }
  else
  {
    qDebug() << "You Pressed Other Key";
  }
}

void VideoWidget::updateTargetRect()
{
  if(hasImage_)
  {
    QSize size = currentImage_.size();

    if(QWidget::size().height() > size.height()
       && QWidget::size().width() > size.width())
    {
      size.scale(QWidget::size().expandedTo(size), Qt::KeepAspectRatio);
    }
    else
    {
       size.scale(QWidget::size().boundedTo(size), Qt::KeepAspectRatio);
    }

    targetRect_ = QRect(QPoint(0, 0), size);
    targetRect_.moveCenter(rect().center());
  }
}
