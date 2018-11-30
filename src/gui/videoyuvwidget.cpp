#include "videoyuvwidget.h"

#include "statisticsinterface.h"

#include <QPaintEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>

const uint16_t GLVIEWBUFFERSIZE = 5;

VideoYUVWidget::VideoYUVWidget(QWidget* parent, uint32_t sessionID, uint8_t borderSize)
  : QOpenGLWidget(parent),
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

  setWindowState(Qt::WindowFullScreen);

  setUpdatesEnabled(true);

  // the new syntax does not work for some reason (unresolved overloaded function type)
  QObject::connect(this, SIGNAL(newImage()), this, SLOT(repaint()));
}

VideoYUVWidget::~VideoYUVWidget()
{
  viewBuffer_.clear();
  dataBuffer_.clear();
}

void VideoYUVWidget::inputImage(std::unique_ptr<uchar[]> data, QImage &image)
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
    if(viewBuffer_.size() > GLVIEWBUFFERSIZE)
    {
      qDebug() << "Buffer full:" << viewBuffer_.size() << "/" <<GLVIEWBUFFERSIZE
               << "Deleting oldest image from viewBuffer in VideoGLWidget:" << sessionID_;
      viewBuffer_.pop_back();
      dataBuffer_.pop_back();
      //stats_->packetDropped("view" + QString::number(sessionID_));
    }
  }

  //update();

  emit newImage();
  drawMutex_.unlock();
}

void VideoYUVWidget::initializeGL()
{
  initializeOpenGLFunctions();

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1600, 896, 0,
                GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

  //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, lastImage_.size().width(), lastImage_.size().height(), 0,
  //              GL_RGB, GL_UNSIGNED_BYTE, nullptr);

  qDebug() << "Opengl Initialization:" << glGetError();
  qDebug() << "Image size:" << lastImage_.size();
}


void VideoYUVWidget::drawOpenGL(bool updateImage)
{
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, texture_);

  if(updateImage)
  {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1600, 896,
                   GL_BGRA,  GL_UNSIGNED_BYTE, dataBuffer_.back().get());
  }

  //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lastImage_.size().width(), lastImage_.size().height(),
  //                 GL_RGB, GL_UNSIGNED_BYTE, dataBuffer_.back().get());

  glBegin(GL_QUADS);              // Each set of 4 vertices form a quad

  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(1.0f, 1.0f);

  //Top-Left
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f, 1.0f);

  //Bottom-Left
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f, -1.0f);

  //Bottom-Right
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(1.0f, -1.0f);
  glEnd();

  glFlush();
  //qDebug() << "Opengl Drawing:" << glGetError();
}


void VideoYUVWidget::paintGL()
{
  if(firstImageReceived_)
  {
    drawMutex_.lock();

    if(!viewBuffer_.empty())
    {
      //qDebug() << "Trying to draw image:" << viewBuffer_.back().size();

      drawOpenGL(true);

      lastImage_ = viewBuffer_.back();
      lastImageData_ = std::move(dataBuffer_.back());
      viewBuffer_.pop_back();
      dataBuffer_.pop_back();
    }
    else
    {
      drawOpenGL(false);
    }

    drawMutex_.unlock();
  }

}



void VideoYUVWidget::resizeEvent(QResizeEvent *event)
{
  qDebug() << "VideoGLWidget resizeEvent:" << sessionID_;
  QOpenGLWidget::resizeEvent(event); // its important to call this resize function, not the qwidget one.
  updateTargetRect();
}

void VideoYUVWidget::updateTargetRect()
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
    QSize frameSize = QWidget::size();

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

    previousSize_ = lastImage_.size();
  }
  else
  {
    qDebug() << "VideoGLWidget: Tried updating target rect before picture";
  }
}

void VideoYUVWidget::keyPressEvent(QKeyEvent *event)
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

void VideoYUVWidget::mouseDoubleClickEvent(QMouseEvent *e) {
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

void VideoYUVWidget::enterFullscreen()
{
  qDebug() << "Setting VideoGLWidget fullscreen";

  tmpParent_ = QWidget::parentWidget();
  this->setParent(nullptr);
  //this->showMaximized();
  this->show();
  this->setWindowState(Qt::WindowFullScreen);
}

void VideoYUVWidget::exitFullscreen()
{
  qDebug() << "Returning GL video widget to original place.";
  this->setParent(tmpParent_);
  //this->showMaximized();
  this->show();
  this->setWindowState(Qt::WindowMaximized);

  emit reattach(sessionID_, this);
}
