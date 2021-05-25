#include "videodrawhelper.h"

#include "common.h"
#include "logger.h"

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
  borderSize_(borderSize),
  currentFrame_(0)
{}

VideoDrawHelper::~VideoDrawHelper()
{
  frameBuffer_.clear();
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

void VideoDrawHelper::inputImage(QWidget* widget, std::unique_ptr<uchar[]> data, QImage &image,
                                 int64_t timestamp)
{
  if(!firstImageReceived_)
  {
    currentFrame_ = timestamp;
    lastFrame_ = {image, std::move(data), timestamp};
    firstImageReceived_ = true;
    updateTargetRect(widget);
  }
  else
  {
    if(previousSize_ != image.size())
    {
      qDebug() << "Drawing," << metaObject()->className()
               << ": Video widget needs to update its target rectangle because of resolution change.";
      frameBuffer_.clear();
      frameBuffer_.push_front({image, std::move(data), timestamp});

      updateTargetRect(widget);
    }
    else
    {
      frameBuffer_.push_front({image, std::move(data), timestamp});
    }

    // delete oldes image if there is too much buffer
    if(frameBuffer_.size() > VIEWBUFFERSIZE)
    {
      if ( widget->isVisible() &&
           widget->isActiveWindow() &&
          !widget->isHidden() &&
          !widget->isMinimized())
      {
        Logger::getLogger()->printWarning(this, "Buffer full when inputting image",
                                          {"Buffer"}, QString::number(frameBuffer_.size()) + "/"
                                                      + QString::number(VIEWBUFFERSIZE));
      }

      frameBuffer_.pop_back();

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
    if(!frameBuffer_.empty())
    {
      image = frameBuffer_.back().image;

      lastFrame_ = std::move(frameBuffer_.back());
      frameBuffer_.pop_back();
      return true;
    }
    else
    {
      image = lastFrame_.image;
    }
  }
  return false;
}

void VideoDrawHelper::updateTargetRect(QWidget* widget)
{
  if(firstImageReceived_)
  {
    Q_ASSERT(lastFrame_.image.data_ptr());
    if(lastFrame_.image.data_ptr() == nullptr)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Null pointer in current image!");
      return;
    }

    QSize size = lastFrame_.image.size();
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

    previousSize_ = lastFrame_.image.size();
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Tried updating target rect before picture");
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
