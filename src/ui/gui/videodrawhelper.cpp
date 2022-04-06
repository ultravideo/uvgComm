#include "videodrawhelper.h"

#include "common.h"
#include "logger.h"

#include <QWidget>
#include <QKeyEvent>
#include <QPainter>

const uint16_t VIEWBUFFERSIZE = 5;

const QImage::Format IMAGE_FORMAT = QImage::Format_ARGB32;

const QColor unselectedColor = QColor(50, 50, 50, 200);
const QColor selectedColor = QColor(255, 255, 255, 0);


VideoDrawHelper::VideoDrawHelper(uint32_t sessionID, uint32_t index, uint8_t borderSize):
  sessionID_(sessionID),
  index_(index),
  tmpParent_(nullptr),
  firstImageReceived_(false),
  previousSize_(QSize(0,0)),
  borderSize_(borderSize),
  currentFrame_(0),
  roiMutex_(),
  roiMask_(nullptr),
  roiSize_(0),
  drawOverlay_(false),
  overlay_()
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
#if QT_VERSION_MAJOR == 6
  palette.setColor(QPalette::Window, Qt::black);
#else
  palette.setColor(QPalette::Background, Qt::black);
#endif
  widget->setPalette(palette);

  widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  //widget->showFullScreen();
  widget->setWindowState(Qt::WindowFullScreen);
  widget->setUpdatesEnabled(true);
}

void VideoDrawHelper::enableOverlay()
{
  drawOverlay_ = true;
}

bool VideoDrawHelper::readyToDraw()
{
  return firstImageReceived_;
}

void VideoDrawHelper::inputImage(QWidget* widget, std::unique_ptr<uchar[]> data, QImage &image,
                                 int64_t timestamp)
{
  if (!widget->isVisible() ||
      widget->isHidden() ||
      widget->isMinimized())
  {
    return;
  }

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
      Logger::getLogger()->printNormal(this, "Video widget needs to update its target "
                                             "rectangle because of resolution change");

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

      Logger::getLogger()->printWarning(this, "Buffer full when inputting image",
                                        {"Buffer"}, QString::number(frameBuffer_.size()) + "/"
                                                    + QString::number(VIEWBUFFERSIZE));


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

    if (drawOverlay_)
    {
      overlay_ = QImage(size, IMAGE_FORMAT);
      overlay_.fill(unselectedColor);
      roiMutex_.lock();
      roiMask_ = nullptr;
      roiMutex_.unlock();
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Tried updating target rect before picture");
  }
}


void VideoDrawHelper::addPointToOverlay(const QPointF& position, bool addPoint, bool removePoint)
{
  if (drawOverlay_ && (addPoint != removePoint))
  {
    const QSizeF size(targetRect_.width()/10, targetRect_.width()/10);
    QPointF circleHalfway(size.width()/2, size.height()/2);

    QPainter painter(&overlay_);

    QBrush brush(selectedColor);

    if (removePoint)
    {
      brush = QBrush(unselectedColor);
    }

    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);

    // need to be set so we can override the destination alpha
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    QRectF drawnCircle(position - targetRect_.topLeft() - circleHalfway, size);
    painter.drawEllipse(drawnCircle);
    roiMutex_.lock();
    roiMask_ = nullptr;
    roiMutex_.unlock();
  }
}


void VideoDrawHelper::drawOverlay(QPainter& painter)
{
  if (drawOverlay_)
  {
    painter.drawImage(getTargetRect(), overlay_);
  }
}


void VideoDrawHelper::keyPressEvent(QWidget* widget, QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    Logger::getLogger()->printNormal(this, "ESC key pressed");

    if(widget->isFullScreen() && sessionID_ != 0)
    {
      exitFullscreen(widget);
    }
  }
  else
  {
    Logger::getLogger()->printNormal(this, "non-ESC key pressed. Doing nothing");
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
  Logger::getLogger()->printNormal(this, "Setting video view fullscreen");

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
  Logger::getLogger()->printNormal(this, "Returning video widget to original place", "SessionID", QString::number(sessionID_));

  widget->setParent(tmpParent_);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowMaximized);

  emit reattach(sessionID_);
}


std::shared_ptr<int8_t[]> VideoDrawHelper::getRoiMask(int width, int height)
{
  if ((drawOverlay_ && roiMask_ == nullptr) || roiSize_ != width*height)
  {
    createROIMask(width, height);
  }

  roiMutex_.lock();
  std::shared_ptr<int8_t[]> mask = roiMask_;
  roiMutex_.unlock();

  return mask;
}


void VideoDrawHelper::createROIMask(int width, int height)
{
  roiSize_ = width*height;
  roiMask_ = std::shared_ptr<int8_t[]> (new int8_t[roiSize_]);

  for (int i = 0; i < height; ++i)
  {
    for (int j = 0; j < width; ++j)
    {
      float widthMultiplier = (float)overlay_.width()/width;
      float heightMultiplier = (float)overlay_.height()/height;

      QPoint imagePosition(widthMultiplier*j, heightMultiplier*i);
      QColor overlayColor = overlay_.pixelColor(imagePosition);

      if (overlayColor == selectedColor)
      {
        roiMask_[i*height + j] = 27;
      }
      else
      {
        roiMask_[i*height + j] = 47;
      }
    }
  }
}
