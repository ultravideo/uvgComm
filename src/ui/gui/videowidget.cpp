#include "videowidget.h"

#include "statisticsinterface.h"
#include "logger.h"

#include <QPaintEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>

VideoWidget::VideoWidget(QWidget* parent, uint32_t sessionID, uint32_t index,
                         uint8_t borderSize)
  : QFrame(parent),
  stats_(nullptr),
  sessionID_(sessionID),
  helper_(sessionID, index, borderSize)
{
  helper_.initWidget(this);

  QFrame::setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  QFrame::setLineWidth(borderSize);
  QFrame::setMidLineWidth(1);

  // the new syntax does not work for some reason (unresolved overloaded function type)
  QObject::connect(this, SIGNAL(newImage()), this, SLOT(repaint()));
  QObject::connect(&helper_, &VideoDrawHelper::detach, this, &VideoWidget::detach);
  QObject::connect(&helper_, &VideoDrawHelper::reattach, this, &VideoWidget::reattach);

  helper_.updateTargetRect(this);
}

VideoWidget::~VideoWidget()
{}

void VideoWidget::enableOverlay()
{
  helper_.enableOverlay();
}

std::shared_ptr<int8_t[]> VideoWidget::getRoiMask(int& width, int& height, int qp, bool scaleToInput)
{
  return helper_.getRoiMask(width, height, qp, scaleToInput);
}

void VideoWidget::inputImage(std::unique_ptr<uchar[]> data, QImage &image,
                             int64_t timestamp)
{
  drawMutex_.lock();
  // if the resolution has changed in video

  helper_.inputImage(this, std::move(data), image, timestamp);

  //update();

  emit newImage();
  drawMutex_.unlock();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);

  if(helper_.readyToDraw())
  {
    drawMutex_.lock();

    if(QFrame::frameRect() != helper_.getFrameRect())
    {
      QFrame::setFrameRect(helper_.getFrameRect());
      QWidget::setMinimumHeight(helper_.getFrameRect().height()*QWidget::minimumWidth()/helper_.getFrameRect().width());
    }

    QImage frame;
    if(helper_.getRecentImage(frame))
    {
      // sessionID 0 is the self display and we are not interested
      // update stats only for each new image.
      if(stats_ && sessionID_ != 0)
      {
        stats_->presentPackage(sessionID_, "Video");
      }
    }

    painter.drawImage(helper_.getTargetRect(), frame);
    helper_.drawOverlay(painter);
    drawMutex_.unlock();
  }
  else
  {
    painter.fillRect(event->rect(), QBrush(QColor(0,0,0)));
  }

  QFrame::paintEvent(event);
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  helper_.updateTargetRect(this);
}


void VideoWidget::keyPressEvent(QKeyEvent *event)
{
  helper_.keyPressEvent(this, event);
}


void VideoWidget::mousePressEvent(QMouseEvent *e)
{
  QWidget::mousePressEvent(e);

  // if you want this to also trigger on movement without pressing,
  // enable mouse tracking in qwidget

  helper_.addPointToOverlay(e->localPos(),
                            e->button() == Qt::LeftButton,
                            e->button() == Qt::RightButton);
}


void VideoWidget::mouseMoveEvent(QMouseEvent *e)
{
  QWidget::mouseMoveEvent(e);

  // if you want this to also trigger on movement without pressing,
  // enable mouse tracking in qwidget

  Qt::MouseButtons buttonFlags = e->buttons();

  helper_.addPointToOverlay(e->localPos(),
                            buttonFlags & Qt::LeftButton,
                            buttonFlags & Qt::RightButton);
}


void VideoWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  QWidget::mouseDoubleClickEvent(e);
  helper_.mouseDoubleClickEvent(this);
}
