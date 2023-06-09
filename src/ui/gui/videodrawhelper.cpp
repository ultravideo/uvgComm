#include "videodrawhelper.h"

#include "common.h"
#include "logger.h"

#include <QWidget>
#include <QKeyEvent>
#include <QPainter>

const uint16_t VIEWBUFFERSIZE = 5;
const QImage::Format IMAGE_FORMAT = QImage::Format_ARGB32;
const int maximumQPChange = 25;
const int CTU_SIZE = 64;


VideoDrawHelper::VideoDrawHelper(uint32_t sessionID, uint8_t borderSize):
  sessionID_(sessionID),
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
  grid_(),
  roiQP_(22),
  backgroundQP_(47),
  brushSize_(2),
  videoResolution_(QSize(0,0)),
  showGrid_(false),
  pixelBasedDrawing_(false),
  micIcon_(QString(":/icons/mic_off.svg")),
  drawIcon_(false),
  fullscreen_(false),
  bufferFullWarnings_(0)
{
  micIcon_.setAspectRatioMode(Qt::KeepAspectRatio);
}


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


void VideoDrawHelper::setDrawMicOff(bool state)
{
  drawIcon_ = state;
}


void VideoDrawHelper::enableOverlay(int roiQP, int backgroundQP,
                                    int brushSize, bool showGrid, bool pixelBased,
                                    QSize videoResolution)
{
  roiMutex_.lock();

  videoResolution_ = videoResolution;
  drawOverlay_ = true;

  roiQP_ = roiQP;
  backgroundQP_ = backgroundQP;

  // grid updates are considered part of the UI so they are updated immediately
  showGrid_ = showGrid;
  drawGrid();

  pixelBasedDrawing_ = pixelBased;
  brushSize_ = brushSize;

  currentMask_ = nullptr;
  roiMutex_.unlock();
}


void VideoDrawHelper::resetOverlay()
{
  roiMutex_.lock();
  overlay_ = QImage(imageRect_.size(), IMAGE_FORMAT);
  overlay_.fill(qpToColor(backgroundQP_));

  drawGrid();

  currentMask_ = nullptr;
  roiMutex_.unlock();
}


void VideoDrawHelper::updateROIMask()
{
  roiMutex_.lock();
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
      if (bufferFullWarnings_ == 0)
      {
        Logger::getLogger()->printWarning(this, "Buffer full when inputting image",
                                         {"Buffer"}, QString::number(frameBuffer_.size()) + "/" +
                                                     QString::number(VIEWBUFFERSIZE));
      }

      ++bufferFullWarnings_;
      frameBuffer_.pop_back();

      //setUpdatesEnabled(true);
      //stats_->packetDropped("view" + QString::number(sessionID_));
    }
    else if (bufferFullWarnings_ > 0)
    {
      Logger::getLogger()->printWarning(this, "Discarded frames because buffer was full",
                                        {"Amount discarded"}, QString::number(bufferFullWarnings_));
      bufferFullWarnings_ = 0;
    }
  }
}

#ifdef KVAZZUP_HAVE_ONNX_RUNTIME
void VideoDrawHelper::inputDetections(std::vector<Detection> detections, QSize original_size, uint64_t timestamp)
{
  auto oldDetections = detectionsBuffer_.back();
  QSizeF viewMultiplier = getSizeMultipliers(videoResolution_.width(),
                                             videoResolution_.height());
  QPointF viewCTUSize = {CTU_SIZE*viewMultiplier.width(), CTU_SIZE*viewMultiplier.height()};
  for (auto& d : detections)
  {
    d.bbox.x *= viewMultiplier.width();
    d.bbox.width *= viewMultiplier.width();
    d.bbox.y *= viewMultiplier.height();
    d.bbox.height *= viewMultiplier.height();
  }

  if(drawOverlay_ && !oldDetections.empty())
  {
    resetOverlay();

    QPainter painter(&overlay_);
    QBrush brush(qpToColor(roiQP_));
    painter.setPen(Qt::white);

    painter.setCompositionMode(QPainter::CompositionMode_Source);


    for (const Detection& d : oldDetections)
    {
      int x_adj = d.bbox.x - floor((d.bbox.x + viewCTUSize.x()/3) / viewCTUSize.x()) * viewCTUSize.x();
      int y_adj = d.bbox.y - floor((d.bbox.y + viewCTUSize.y()/3) / viewCTUSize.y()) * viewCTUSize.y();
      int w_adj = -(d.bbox.width + d.bbox.x) + ceil((d.bbox.width+d.bbox.x - viewCTUSize.x()/3) / viewCTUSize.x()) * viewCTUSize.x() + x_adj;
      int h_adj = -(d.bbox.height + d.bbox.y) + ceil((d.bbox.height+d.bbox.y - viewCTUSize.y()/3) / viewCTUSize.y()) * viewCTUSize.y() + y_adj;
      painter.fillRect(d.bbox.x-x_adj,
                       d.bbox.y-y_adj,
                       d.bbox.width+w_adj,
                       d.bbox.height+h_adj,
                       brush);
    }
    for (const Detection& d : oldDetections)
    {
      painter.drawRect(d.bbox.x, d.bbox.y, d.bbox.width, d.bbox.height);
    }
    painter.end();
  }

  for (size_t i = detectionsBuffer_.size()-1; i > 0; i--)
  {
    detectionsBuffer_[i] = detectionsBuffer_[i-1];
  }

}
#endif

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
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                      "Null pointer in current image!");
      return;
    }

    QSize imageSize = lastFrame_.image.size();
    QSize maxImageArea = widget->size() - QSize(borderSize_, borderSize_)*2;

    // draw borders at the edges by default, may need some adjustment depending on the image size
    borderRect_ = widget->rect();
    borderRect_.setSize(borderRect_.size() - QSize(1,1));


    // Aspect ratios can be used to determine which is the best limiting factor 
    // so all of the widget space can be used.
    float widgetAspectRatio = (float)maxImageArea.width()/maxImageArea.height();
    float frameAspectRatio =  (float)imageSize.width()/imageSize.height();

    // by default we use the image as our borders, but in some cases the best way to display
    // the image is to only show part of it in which case we use the widget border
    bool useImageLeft = true;
    bool useImageTop = true;

    if(maxImageArea.height() > imageSize.height()
       && maxImageArea.width() > imageSize.width())
    {
      // Scale up the frame because it is too small to fill the widget slot (will stretch the image, but not lose pixels).
      // Mostly relevant for very small resolutions (and big screens) where we fill the widget space as much as we can
      // while also showing all the pixels available, maximizing the little quality we have without forcing
      // the user to look at a postage stamp video.
      imageSize.scale(maxImageArea.expandedTo(imageSize), Qt::KeepAspectRatio);
      borderRect_.setSize(imageSize + QSize(1,1));
    }
    else if (drawOverlay_)
    {
      imageSize.scale(maxImageArea.boundedTo(imageSize), Qt::KeepAspectRatio);
      borderRect_.setSize(imageSize + QSize(1,1));
    }
    else if (widgetAspectRatio <= frameAspectRatio)
    {
      // Limit the target by widget height without stretching the image
      // (will cut some of the image from left and right)
      QSize newScale = {imageSize.width(), maxImageArea.height()};
      imageSize.scale(newScale, Qt::KeepAspectRatio);

      if (imageSize.height() < maxImageArea.height())
      {
        borderRect_.setHeight(imageSize.height());
      }
      useImageLeft = false;
    }
    else
    {
      // Limit the target by widget width without stretching the image
      // (will cut some of the image from top and bottom)
      QSize newScale = {maxImageArea.width(), imageSize.height()};
      imageSize.scale(newScale, Qt::KeepAspectRatio);

      if (imageSize.width() < maxImageArea.width())
      {
        borderRect_.setWidth(imageSize.width());
      }

      useImageTop = false;
    }

    imageRect_ = QRect(QPoint(0, 0), imageSize);
    imageRect_.moveCenter(widget->rect().center());

    if (useImageLeft)
    {
      borderRect_.moveLeft(imageRect_.left() - 1);
    }
    if (useImageTop)
    {
      borderRect_.moveTop(imageRect_.top() - 1);
    }

    QPoint iconLocation = QPoint(0, 0);
    if (imageRect_.topLeft().x() > iconLocation.x())
    {
      iconLocation.setX(imageRect_.topLeft().x());
    }
    if (imageRect_.topLeft().y() > iconLocation.y())
    {
      iconLocation.setY(imageRect_.topLeft().y());
    }

    iconRect_ = QRect(iconLocation, imageSize*0.05);

    previousSize_ = lastFrame_.image.size();

    if (drawOverlay_)
    {
      resetOverlay();
    }
  }
  else
  {
    //Logger::getLogger()->printWarning(this, "Tried updating target rect before picture");
  }
}

void VideoDrawHelper::addPointToOverlay(const QPointF& position, bool addPoint, bool removePoint)
{
  if (drawOverlay_ && (addPoint != removePoint))
  {
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

    if (pixelBasedDrawing_)
    {
      int proportion = 20; // 5% of image

      if (brushSize_ == 2)
      {
        proportion = 10; // 10% of image
      }
      else if (brushSize_ >= 3)
      {
        proportion = 5;  // 20% of image
      }

      const QSizeF size(imageRect_.width()/proportion, imageRect_.width()/proportion);
      QPointF circleHalfway(size.width()/2, size.height()/2);
      QRectF drawnCircle(position - imageRect_.topLeft() - circleHalfway, size);

      painter.drawEllipse(drawnCircle);
    }
    else
    {
      QSizeF viewMultiplier = getSizeMultipliers(videoResolution_.width(),
                                                 videoResolution_.height());
      QPointF viewCTUSize = {CTU_SIZE*viewMultiplier.width(), CTU_SIZE*viewMultiplier.height()};
      QPointF viewPosition = (position - imageRect_.topLeft());

      // color the CTU at mouse coordinates
      setCTUQP(painter, viewPosition, viewMultiplier); // center

      if (brushSize_ >= 2)
      {
        // CTUs next to mouse click
        setCTUQP(painter, viewPosition + QPointF{0,               -viewCTUSize.y()},
                 viewMultiplier); // top
        setCTUQP(painter, viewPosition + QPointF{-viewCTUSize.x(), 0}              ,
                 viewMultiplier); // left

        setCTUQP(painter, viewPosition + QPointF{ viewCTUSize.x(),  0}             ,
                 viewMultiplier); // right
        setCTUQP(painter, viewPosition + QPointF{               0, viewCTUSize.y()},
                 viewMultiplier); // bottom
      }
      if (brushSize_ >= 3)
      {
        // corners
        setCTUQP(painter, viewPosition - viewCTUSize                               ,
                 viewMultiplier); // top left
        setCTUQP(painter, viewPosition + QPointF{viewCTUSize.x(), -viewCTUSize.y()},
                 viewMultiplier); // top right
        setCTUQP(painter, viewPosition + QPointF{-viewCTUSize.x(), viewCTUSize.y()},
                 viewMultiplier); // bottom left
        setCTUQP(painter, viewPosition + viewCTUSize                               ,
                 viewMultiplier); // bottom right
      }
    }

    roiMutex_.unlock();
  }
}


void VideoDrawHelper::setCTUQP(QPainter& painter, const QPointF& viewPosition, QSizeF viewMultiplier)
{
  // here we get the area/rectangle in video frame coordinates rather than view coordinates
  // because doing it correctly in view coordinates is much more difficult

  // first we translate the point in view to our video
  QPointF pointInVideo;

  pointInVideo.setX(viewPosition.x()/viewMultiplier.width());
  pointInVideo.setY(viewPosition.y()/viewMultiplier.height());

  // then we get the CTU rectangle within the video using modulo
  // (since we know that video CTU size is contant).
  QRectF videoCTU;

  videoCTU.setLeft(pointInVideo.x() - fmod(pointInVideo.x(), CTU_SIZE));
  videoCTU.setTop(pointInVideo.y() - fmod(pointInVideo.y(), CTU_SIZE));
  videoCTU.setSize({CTU_SIZE, CTU_SIZE});

  // then we translate the video CTU rectangle to our view as a rectangle
  QRectF viewCTU;

  viewCTU.setLeft(videoCTU.left()*viewMultiplier.width());
  viewCTU.setTop(videoCTU.top()*viewMultiplier.height());

  viewCTU.setWidth(videoCTU.width()*viewMultiplier.width());
  viewCTU.setHeight(videoCTU.height()*viewMultiplier.height());

  // lastly we draw the view rectangle
  painter.drawRect(viewCTU);
}


void VideoDrawHelper::drawGrid()
{
  if (drawOverlay_ && firstImageReceived_)
  {
    // reset the grid in every case, this also clears the grid in case it was disabled
    grid_ = QImage(imageRect_.size(), IMAGE_FORMAT);
    grid_.fill(QColor(0,0,0,0));

    if (showGrid_)
    {
      QSizeF multiplier = getSizeMultipliers(videoResolution_.width(), videoResolution_.height());

      // first we generate the lines for drawing
      QVector<QLine> lines;

      // specify vertical lines
      for (unsigned int i = 0; i < videoResolution_.width(); ++i)
      {
        if (i%64 == 0 || i%64 == 63)
        {
          int x1 = multiplier.width()*i;
          int y1 = 0;
          int x2 = x1;
          int y2 = grid_.height();

          lines.push_back(QLine(x1, y1, x2, y2));
        }
      }

      // specify horizontal lines
      for (unsigned int j = 0; j < videoResolution_.height(); ++j)
      {
        if (j%CTU_SIZE == 0 || j%CTU_SIZE == 63)
        {
          int x1 = 0;
          int y1 = multiplier.height()*j;
          int x2 = grid_.width();
          int y2 = y1;

          lines.push_back(QLine(x1, y1, x2, y2));
        }
      }

      // draw the lines
      QPainter painter(&grid_);
      painter.drawLines(lines);
    }
  }
}


void VideoDrawHelper::draw(QPainter& painter)
{
  if (drawOverlay_)
  {
    painter.drawImage(getTargetRect(), overlay_);
    painter.drawImage(getTargetRect(), grid_);
  }

  if (drawIcon_)
  {
    micIcon_.render(&painter, iconRect_);
  }

  if (borderSize_ > 0 && !fullscreen_)
  {
    QPen borderPen(QColor(140, 140, 140, 255));
    painter.setPen(borderPen);
    painter.drawRect(borderRect_);
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


void VideoDrawHelper::mouseDoubleClickEvent(QWidget* widget)
{
  if(sessionID_ != 0)
  {
    if(widget->isFullScreen())
    {
      exitFullscreen(widget);
    }
    else
    {
      enterFullscreen(widget);
    }
  }
}


void VideoDrawHelper::enterFullscreen(QWidget* widget)
{
  Logger::getLogger()->printNormal(this, "Setting video view fullscreen");
  fullscreen_ = true;

  tmpParent_ = widget->parentWidget();
  widget->setParent(nullptr);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowFullScreen);

  emit detach(sessionID_);
  widget->raise();
}


void VideoDrawHelper::exitFullscreen(QWidget* widget)
{
  Logger::getLogger()->printNormal(this, "Returning video widget to original place", "SessionID",
                                   QString::number(sessionID_));

  fullscreen_ = false;

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

    // copy the ROI mask in memory because it is faster than always creating a new one
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
  const int halfQPOffset = CTU_SIZE/2;

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
  QSizeF multipliers = getSizeMultipliers(width, height);

  // write the QP values to ROI map
  for (int i = 0; i < height - halfQPOffset; ++i)
  {
    for (int j = 0; j < width - halfQPOffset; ++j)
    {
      // calculate position in overlay
      QPoint imagePosition(multipliers.width()*(j + halfQPOffset), multipliers.height()*(i + halfQPOffset));
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


QSizeF VideoDrawHelper::getSizeMultipliers(int width, int height)
{
  return QSizeF((float)imageRect_.width()/width, (float)imageRect_.height()/height);
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
  int qp = color.alpha()*maximumQPChange/200 + 22 - baseQP;
  clipValue(qp, maximumQPChange);
  return qp;
}


QColor VideoDrawHelper::qpToColor(int qp)
{
  if (qp == 22)
  {
    return QColor(0,0,0,0);
  }

  return QColor(75, 75, 75, (qp - 22)*200/maximumQPChange);
}
