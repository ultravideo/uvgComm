#include "videodrawhelper.h"

#include "common.h"
#include "logger.h"

#include <QWidget>
#include <QKeyEvent>
#include <QPainter>

const uint16_t VIEWBUFFERSIZE = 5;

const QImage::Format IMAGE_FORMAT = QImage::Format_ARGB32;

const QColor unselectedColor = QColor(50, 50, 50, 200);
const QColor selectedColor = QColor(0, 0, 0, 0);

const int maximumQPChange = 25;


VideoDrawHelper::VideoDrawHelper(uint32_t sessionID, uint32_t index, uint8_t borderSize):
  sessionID_(sessionID),
  index_(index),
  tmpParent_(nullptr),
  firstImageReceived_(false),
  previousSize_(QSize(0,0)),
  borderSize_(borderSize),
  currentFrame_(0),
  roiMutex_(),
  currentSize_(0),
  currentMask_(nullptr),
  drawOverlay_(false),
  overlay_(),
  roiQP_(22),
  backgroundQP_(47)
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


void VideoDrawHelper::enableOverlay(int roiQP, int backgroundQP)
{
  roiMutex_.lock();
  drawOverlay_ = true;
  roiQP_ = roiQP;
  backgroundQP_ = backgroundQP;
  currentMask_ = nullptr;
  roiMutex_.unlock();
}


void VideoDrawHelper::resetOverlay()
{
  roiMutex_.lock();
  overlay_ = QImage(targetRect_.size(), IMAGE_FORMAT);
  overlay_.fill(qpToColor(backgroundQP_));

  currentMask_ = nullptr;
  roiMutex_.unlock();
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
      resetOverlay();
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

    roiMutex_.lock();
    QPainter painter(&overlay_);

    QBrush brush(qpToColor(roiQP_));

    if (removePoint)
    {
      brush = QBrush(qpToColor(backgroundQP_));
    }

    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);

    // need to be set so we can override the destination alpha
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    QRectF drawnCircle(position - targetRect_.topLeft() - circleHalfway, size);
    painter.drawEllipse(drawnCircle);

    currentMask_ = nullptr;
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
  Logger::getLogger()->printNormal(this, "Returning video widget to original place", "SessionID",
                                   QString::number(sessionID_));

  widget->setParent(tmpParent_);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowMaximized);

  emit reattach(sessionID_);
}


std::unique_ptr<int8_t[]> VideoDrawHelper::getRoiMask(int& width, int& height,
                                                      int qp, bool scaleToInput)
{
  std::unique_ptr<int8_t[]> roiMask = nullptr;

  if (drawOverlay_)
  {
    roiMutex_.lock();

    if (currentMask_ == nullptr || width*height != currentSize_)
    {
      updateROIMask(width, height, qp, scaleToInput);
    }

    if (currentMask_)
    {
      roiMask = std::unique_ptr<int8_t[]> (new int8_t[currentSize_]);
      memcpy(roiMask.get(), currentMask_.get(), currentSize_);
    }

    roiMutex_.unlock();
  }
  else
  {
    Logger::getLogger()->printProgramError(this,
                                           "Trying to get ROI mask from view without overlay");
  }

  return roiMask;
}


void VideoDrawHelper::updateROIMask(int &width, int &height, int qp, bool scaleToInput)
{
  // This offset is required because Kvazaar reads the ROI map from the left corner instead
  // of the CU center and this offset correct for this error between overlay and ROI map
  const int halfQPOffset = 32;

  // These are needed for this to work. Sometimes the overlay has not been created yet when the
  // first update call arrives so this takes care of that situation
  if (overlay_.width() == 0 || overlay_.height() == 0 ||
      width < halfQPOffset || height < halfQPOffset)
  {
    currentSize_ = 0;
    currentMask_ = nullptr;
    return;
  }

  // Does not seem to work with kvazaar atm, but probably a smaller ROI map would
  // also work than the whole frame (if divided by CU size for example)
  if (!scaleToInput)
  {
    width = overlay_.width();
    height = overlay_.height();
  }

  // allocate memory for the mask. This is the mask we hold in memory and use to
  // make copies for kvazaar to use.
  currentSize_ = width*height;
  currentMask_ = std::unique_ptr<int8_t[]> (new int8_t[currentSize_]);

  // if the overlay is different size, we need to offset this difference
  float widthMultiplier = (float)overlay_.width()/width;
  float heightMultiplier = (float)overlay_.height()/height;

  // write the QP values to ROI map
  for (int i = 0; i < height - halfQPOffset; ++i)
  {
    for (int j = 0; j < width - halfQPOffset; ++j)
    {
      // calculate position in overlay
      QPoint imagePosition(widthMultiplier*(j + halfQPOffset), heightMultiplier*(i + halfQPOffset));
      QColor overlayColor = overlay_.pixelColor(imagePosition);

      currentMask_[i*width + j] = colorToQP(overlayColor, qp);
    }
  }

  // zero the upper corner just in case, even though it is not used by kvazaar
  for (int i = height - halfQPOffset; i < height; ++i)
  {
    for (int j = width - halfQPOffset; j < width; ++j)
    {
      currentMask_[i*width + j] = 0;
    }
  }

  return;
}


void VideoDrawHelper::clipValue(int& value, int maximumChange)
{
  if (value > maximumChange)
  {
    Logger::getLogger()->printWarning(this, "Clipping QP value because it increases too much");
    value = maximumChange;
  }
  else if (value < -maximumChange)
  {
    Logger::getLogger()->printWarning(this, "Clipping QP value because it decreases too much");
    value = -maximumChange;
  }
}


int VideoDrawHelper::colorToQP(QColor& color, int baseQP)
{
  int qp = color.alpha()*25/200 + 22 - baseQP;
  clipValue(qp, maximumQPChange);
  return qp;
}


QColor VideoDrawHelper::qpToColor(int qp)
{
  if (qp == 22)
  {
    return QColor(0,0,0,0);
  }

  return QColor(50, 50, 50, (qp - 22)*200/25);
}
