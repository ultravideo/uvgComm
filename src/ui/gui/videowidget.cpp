#include "videowidget.h"

#include "statisticsinterface.h"

#include "logger.h"

#include <QPaintEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QProcessEnvironment>


bool isLocalDisplay() {
  QString display = QProcessEnvironment::systemEnvironment().value("DISPLAY");
  // Avoid enabling flags if using X11 forwarding (e.g., DISPLAY=:10.0 or localhost:10.0)
  return !(display.contains("localhost") || display.contains(":10"));
}


VideoWidget::VideoWidget(QWidget* parent, uint32_t sessionID,
                         LayoutID layoutID, uint8_t borderSize)
  : QWidget(parent),
  stats_(nullptr),
  sessionID_(sessionID),
  cname_(""),
  helper_(sessionID, layoutID, borderSize)
{
  helper_.initWidget(this);

  // One-off informational log if headless forced offscreen mode is enabled
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  QString headlessVal = env.value("KV_HEADLESS_FORCE_OFFSCREEN");
  if (!headlessVal.isEmpty())
  {
    Logger::getLogger()->printNormal(this, "KV_HEADLESS_FORCE_OFFSCREEN enabled",
                                     {"Value"}, {headlessVal});
  }

  Logger::getLogger()->printNormal(this, "VideoWidget created",
                                  {"SessionID", "LayoutID", "WidgetPtr"},
                                  {QString::number(sessionID_), QString::number(layoutID), QString::number((qintptr)this)});

  QObject::connect(&updateTimer_, &QTimer::timeout, this, &VideoWidget::paintTimer);
  updateTimer_.start(16); // 16 ms is the screen refresh time for 60 hz monitors

  // the new syntax does not work for some reason (unresolved overloaded function type)
  QObject::connect(&helper_, &VideoDrawHelper::detach, this, &VideoWidget::detach);
  QObject::connect(&helper_, &VideoDrawHelper::reattach, this, &VideoWidget::reattach);

  helper_.updateTargetRect(this);

  if (isLocalDisplay()) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_PaintOnScreen);
  }
}


VideoWidget::~VideoWidget()
{}


void VideoWidget::drawMicOffIcon(bool status)
{
  helper_.setDrawMicOff(status);
}


void VideoWidget::enableOverlay(int roiQP, int backgroundQP,
                                int brushSize, bool showGrid, bool pixelBased, QSize videoResolution)
{
  helper_.enableOverlay(roiQP, backgroundQP, brushSize, showGrid, pixelBased, videoResolution);
}

void VideoWidget::disableOverlay()
{
  helper_.disableOverlay();
}

void VideoWidget::resetOverlay()
{
  helper_.resetOverlay();
}


std::unique_ptr<int8_t[]> VideoWidget::getRoiMask(int& width, int& height,
                                                  int qp, bool scaleToInput)
{
  return helper_.getRoiMask(width, height, qp, scaleToInput);
}


void VideoWidget::visualizeROIMap(RoiMap& map, int qp)
{
  helper_.visualizeROIMap(map, qp);
}

void VideoWidget::inputImage(std::unique_ptr<uchar[]> data,
                             QImage &image,
                             double framerate,
                             int64_t creationTimestamp,
                             int64_t displayTimestamp)
{
  drawMutex_.lock();
  // if the resolution has changed in video

  helper_.inputImage(this, std::move(data), image, framerate, creationTimestamp, displayTimestamp);

  //update();

  emit newImage();
  drawMutex_.unlock();
}

void VideoWidget::setStats(StatisticsInterface* stats, QString cname)
{
  stats_ = stats;
  cname_ = cname;

  Logger::getLogger()->printNormal(this, "VideoWidget::setStats",
                                  {"SessionID", "CName", "WidgetPtr"},
                                  {QString::number(sessionID_), cname_, QString::number((qintptr)this)});
}


void VideoWidget::inputDetections(std::vector<Detection> detections, QSize original_size, int64_t timestamp)
{
  drawMutex_.lock();
  helper_.inputDetections(detections, original_size, timestamp);
  drawMutex_.unlock();
}


void VideoWidget::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter.setRenderHint(QPainter::Antialiasing, false);

  if(helper_.readyToDraw())
  {
    QImage frame;
    int64_t latency = 0;
    int64_t timestamp = 0;
    bool showLatency = false;
    drawMutex_.lock();
    if(helper_.getRecentImage(frame, timestamp, latency, showLatency))
    {
      // sessionID 0 is the self display and we are not interested
      // update stats only for each new image.
      if(stats_ && sessionID_ != 0)
      {
        stats_->videoLatency(sessionID_, cname_, timestamp, latency);
      }
    }
    drawMutex_.unlock();

    painter.drawImage(helper_.getTargetRect(), frame);

    helper_.draw(painter);

    if (latency != 0 && showLatency)
    {
      painter.setPen(QColor::fromRgb(255, 255, 255));
      painter.drawText(QPoint(width()/2, height()/2), QString::number(latency) + " ms");
    }

  }
  else
  {
    painter.fillRect(event->rect(), QBrush(QColor(0,0,0)));
  }

  //painter.end();  // Force the painter to flush immediately

  QWidget::paintEvent(event);
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

  helper_.addPointToOverlay(e->position(),
                            e->button() == Qt::LeftButton,
                            e->button() == Qt::RightButton);
}


void VideoWidget::mouseReleaseEvent(QMouseEvent *e)
{
  helper_.updateROIMask();
}


void VideoWidget::mouseMoveEvent(QMouseEvent *e)
{
  QWidget::mouseMoveEvent(e);

  // if you want this to also trigger on movement without pressing,
  // enable mouse tracking in qwidget

  Qt::MouseButtons buttonFlags = e->buttons();

  helper_.addPointToOverlay(e->position(),
                            buttonFlags & Qt::LeftButton,
                            buttonFlags & Qt::RightButton);
}


void VideoWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  QWidget::mouseDoubleClickEvent(e);
  helper_.mouseDoubleClickEvent(this);
}


void VideoWidget::paintTimer()
{
  if (!helper_.haveFrames())
  {
    return;
  }

  // If a DISPLAY is set (local or X-forwarded) and the user did not request forced offscreen,
  // use normal repaint to show frames on that display. Otherwise, fall back to offscreen rendering
  // when running experiments with KV_HEADLESS_FORCE_OFFSCREEN=1.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  bool forceOffscreen = !env.value("KV_HEADLESS_FORCE_OFFSCREEN").isEmpty();
  QString display = env.value("DISPLAY");

  // On Windows (and other non-X platforms) there is no DISPLAY env var — assume a display exists
  // and use the normal repaint path unless the user explicitly forced offscreen mode.
#if defined(Q_OS_WIN) || defined(Q_OS_WIN32) || defined(Q_OS_WIN64) || defined(Q_OS_MAC)
  bool hasDisplay = true;
#else
  bool hasDisplay = !display.isEmpty();
#endif

  if (hasDisplay && !forceOffscreen)
  {
    repaint();
    return;
  }

  // Headless / offscreen path: render the next frame into an offscreen QImage
  QImage frame;
  int64_t latency = 0;
  int64_t timestamp = 0;
  bool showLatency = false;

  drawMutex_.lock();
  if (helper_.getRecentImage(frame, timestamp, latency, showLatency))
  {
    // sessionID 0 is the self display and we are not interested
    if (stats_ && sessionID_ != 0)
    {
      stats_->videoLatency(sessionID_, cname_, timestamp, latency);
    }
  }
  drawMutex_.unlock();

  // Render overlays and other decorations into an offscreen image sized like the widget
  QImage offscreen(size(), QImage::Format_ARGB32);
  offscreen.fill(QColor(0,0,0));
  QPainter painter(&offscreen);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter.setRenderHint(QPainter::Antialiasing, false);

  QRect target = helper_.getTargetRect();
  painter.drawImage(target, frame);
  helper_.draw(painter);
  painter.end();
}
