#include "videowidget.h"

#include "statisticsinterface.h"

#include <QPaintEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>

uint16_t VIEWBUFFERSIZE = 5;

VideoWidget::VideoWidget(QWidget* parent, uint32_t sessionID, uint8_t borderSize)
  : QFrame(parent),
  firstImageReceived_(false),
  previousSize_(QSize(0,0)),
  stats_(nullptr),
  sessionID_(sessionID),
  borderSize_(borderSize),
  tmpParent_(nullptr)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground, true);

  QPalette palette = this->palette();
  palette.setColor(QPalette::Background, Qt::black);
  setPalette(palette);

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  QFrame::setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  QFrame::setLineWidth(borderSize_);
  QFrame::setMidLineWidth(1);

  //showFullScreen();
  setWindowState(Qt::WindowFullScreen);

  setUpdatesEnabled(true);

  // the new syntax does not work for some reason (unresolved overloaded function type)
  QObject::connect(this, SIGNAL(newImage()), this, SLOT(repaint()));
}

VideoWidget::~VideoWidget()
{
  viewBuffer_.clear();
  dataBuffer_.clear();
}

void VideoWidget::inputImage(std::unique_ptr<uchar[]> data, QImage &image)
{
  drawMutex_.lock();
  // if the resolution has changed in video

  if(!firstImageReceived_)
  {
    lastImage_ = image;
    lastImageData_ = std::move(data);
    firstImageReceived_ = true;
    updateTargetRect();
  }
  else
  {
    if(previousSize_ != image.size())
    {
      qDebug() << "Video widget needs to update its target rectangle because of resolution change.";
      viewBuffer_.clear();
      dataBuffer_.clear();

      viewBuffer_.push_front(image);
      dataBuffer_.push_front(std::move(data));
      updateTargetRect();
    }
    else
    {
      viewBuffer_.push_front(image);
      dataBuffer_.push_front(std::move(data));
    }

    // delete oldes image if there is too much buffer
    if(viewBuffer_.size() > VIEWBUFFERSIZE)
    {
      qDebug() << "Buffer full:" << viewBuffer_.size() << "/" <<VIEWBUFFERSIZE
               << "Deleting oldest image from viewBuffer in videowidget:" << sessionID_;
      viewBuffer_.pop_back();
      dataBuffer_.pop_back();

      setUpdatesEnabled(true);
      // TODO: There is a possibility of image freezing

      stats_->packetDropped("view" + QString::number(sessionID_));
    }
  }

  //update();

  emit newImage();
  drawMutex_.unlock();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  //qDebug() << "PaintEvent for widget:" << sessionID_;
  QPainter painter(this);

  if(firstImageReceived_)
  {
    drawMutex_.lock();
    if(QFrame::frameRect() != newFrameRect_)
    {
      QFrame::setFrameRect(newFrameRect_);
      QWidget::setMinimumHeight(newFrameRect_.height()*QWidget::minimumWidth()/newFrameRect_.width());
    }

    if(!viewBuffer_.empty())
    {
      painter.drawImage(targetRect_, viewBuffer_.back());
      // sessionID 0 is the self display and we are not interested
      // update stats only for each new image.
      if(stats_ && sessionID_ != 0)
      {
        stats_->presentPackage(sessionID_, "Video");
      }
      lastImage_ = viewBuffer_.back();
      lastImageData_ = std::move(dataBuffer_.back());
      viewBuffer_.pop_back();
      dataBuffer_.pop_back();

    }
    else
    {
      painter.drawImage(targetRect_, lastImage_);
    }

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
  qDebug() << "VideoWidget resizeEvent:" << sessionID_;
  QWidget::resizeEvent(event);
  updateTargetRect();
}

void VideoWidget::updateTargetRect()
{
  if(firstImageReceived_)
  {
    Q_ASSERT(lastImage_.data_ptr());
    if(lastImage_.data_ptr() == nullptr)
    {
      qWarning() << "WARNING: Null pointer in current image!";
      return;
    }

    QSize size = lastImage_.size();
    QSize frameSize = QWidget::size() - QSize(borderSize_,borderSize_);

    if(frameSize.height() > size.height()
       && frameSize.width() > size.width())
    {
      size.scale(frameSize.expandedTo(size), Qt::KeepAspectRatio);
    }
    else
    {
       size.scale(frameSize.boundedTo(size), Qt::KeepAspectRatio);
    }

    targetRect_ = QRect(QPoint(0, 0), size);
    targetRect_.moveCenter(rect().center());
    newFrameRect_ = QRect(QPoint(0, 0), size + QSize(borderSize_,borderSize_));
    newFrameRect_.moveCenter(rect().center());

    previousSize_ = lastImage_.size();
  }
  else
  {
    qDebug() << "VideoWidget: Tried updating target rect before picture";
  }
}

void VideoWidget::keyPressEvent(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    qDebug() << "Esc key pressed";
    if(isFullScreen() && sessionID_ != 0)
    {
      exitFullscreen();
    }
  }
  else
  {
    qDebug() << "You Pressed Other Key";
  }
}


void VideoWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  QWidget::mouseDoubleClickEvent(e);
  if(sessionID_ != 0)
  {
    if(isFullScreen())
    {
      exitFullscreen();
    } else {
      enterFullscreen();
    }
  }
}

void VideoWidget::enterFullscreen()
{
  qDebug() << "Setting videowidget fullscreen";

  QFrame::setFrameStyle(QFrame::NoFrame);

  tmpParent_ = QWidget::parentWidget();
  this->setParent(nullptr);
  //this->showMaximized();
  this->show();
  this->setWindowState(Qt::WindowFullScreen);
}

void VideoWidget::exitFullscreen()
{
  qDebug() << "Returning video widget to original place.";
  this->setParent(tmpParent_);
  //this->showMaximized();
  this->show();
  this->setWindowState(Qt::WindowMaximized);

  QFrame::setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

  emit reattach(sessionID_, this);
}
