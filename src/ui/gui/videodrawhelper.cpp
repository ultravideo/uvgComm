#include "videodrawhelper.h"

#include "common.h"

#include <QDebug>
#include <QWidget>
#include <QKeyEvent>

const uint16_t VIEWBUFFERSIZE = 5;

VideoDrawHelper::VideoDrawHelper(uint32_t sessionID, uint32_t index, uint8_t borderSize):
  sessionID_(sessionID),
  index_(index),
  tmpParent_(nullptr),
  firstImageReceived_(false),
  previousSize_(QSize(0,0)),
  borderSize_(borderSize)
{}

VideoDrawHelper::~VideoDrawHelper()
{
  viewBuffer_.clear();
  dataBuffer_.clear();
}

void VideoDrawHelper::initWidget(QWidget* widget)
{
  widget->setAutoFillBackground(false);
  widget->setAttribute(Qt::WA_NoSystemBackground, true);

  QPalette palette = widget->palette();
  palette.setColor(QPalette::Background, Qt::black);
  widget->setPalette(palette);

  widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  //widget->showFullScreen();
  widget->setWindowState(Qt::WindowFullScreen);
  widget->setUpdatesEnabled(true);
}

bool VideoDrawHelper::readyToDraw()
{
  return firstImageReceived_;
}

void VideoDrawHelper::inputImage(QWidget* widget, std::unique_ptr<uchar[]> data, QImage &image)
{
  if(!firstImageReceived_)
  {
    lastImage_ = image;
    lastImageData_ = std::move(data);
    firstImageReceived_ = true;
    updateTargetRect(widget);
  }
  else
  {
    if(previousSize_ != image.size())
    {
      qDebug() << "Drawing," << metaObject()->className() << ": Video widget needs to update its target rectangle because of resolution change.";
      viewBuffer_.clear();
      dataBuffer_.clear();

      viewBuffer_.push_front(image);
      dataBuffer_.push_front(std::move(data));

      updateTargetRect(widget);
    }
    else
    {
      viewBuffer_.push_front(image);
      dataBuffer_.push_front(std::move(data));
    }

    // delete oldes image if there is too much buffer
    if(viewBuffer_.size() > VIEWBUFFERSIZE)
    {
      qDebug() << "Drawing," << metaObject()->className() << ": Buffer full:" << viewBuffer_.size() << "/" <<VIEWBUFFERSIZE
               << "Deleting oldest image from viewBuffer in videowidget:" << sessionID_;
      viewBuffer_.pop_back();
      dataBuffer_.pop_back();

      //setUpdatesEnabled(true);

      //stats_->packetDropped("view" + QString::number(sessionID_));
    }
  }
}

bool VideoDrawHelper::getRecentImage(QImage& image)
{
  Q_ASSERT(readyToDraw());
  if(readyToDraw())
  {
    if(!viewBuffer_.empty())
    {
      image = viewBuffer_.back();

      lastImage_ = viewBuffer_.back();
      lastImageData_ = std::move(dataBuffer_.back());
      viewBuffer_.pop_back();
      dataBuffer_.pop_back();
      return true;
    }
    else
    {
      image = lastImage_;
    }
  }
  return false;
}

void VideoDrawHelper::updateTargetRect(QWidget* widget)
{
  if(firstImageReceived_)
  {
    Q_ASSERT(lastImage_.data_ptr());
    if(lastImage_.data_ptr() == nullptr)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "Null pointer in current image!");
      return;
    }

    QSize size = lastImage_.size();
    QSize frameSize = widget->size() - QSize(borderSize_,borderSize_);

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
    targetRect_.moveCenter(widget->rect().center());
    newFrameRect_ = QRect(QPoint(0, 0), size + QSize(borderSize_,borderSize_));
    newFrameRect_.moveCenter(widget->rect().center());

    previousSize_ = lastImage_.size();
  }
  else
  {
    qDebug() << "Drawing," << metaObject()->className()
             << ": Tried updating target rect before picture";
  }
}


void VideoDrawHelper::keyPressEvent(QWidget* widget, QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    qDebug() << "Drawing," << metaObject()->className() << ": Esc key pressed";
    if(widget->isFullScreen() && sessionID_ != 0)
    {
      exitFullscreen(widget);
    }
  }
  else
  {
    qDebug() << "Drawing," << metaObject()->className() << ": You Pressed non-ESC Key";
  }
}

void VideoDrawHelper::mouseDoubleClickEvent(QWidget* widget) {
  if(sessionID_ != 0)
  {
    if(widget->isFullScreen())
    {
      exitFullscreen(widget);
    } else {
      enterFullscreen(widget);
    }
  }
}

void VideoDrawHelper::enterFullscreen(QWidget* widget)
{
  qDebug() << "Drawing," << metaObject()->className() << ": Setting VideoGLWidget fullscreen";

  tmpParent_ = widget->parentWidget();
  widget->setParent(nullptr);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowFullScreen);

  emit detach(sessionID_, index_, widget);
  widget->raise();
}


void VideoDrawHelper::exitFullscreen(QWidget* widget)
{
  qDebug() << "Drawing," << metaObject()->className() << ": Returning video widget" << sessionID_ << "to original place.";
  widget->setParent(tmpParent_);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowMaximized);

  emit reattach(sessionID_);
}
