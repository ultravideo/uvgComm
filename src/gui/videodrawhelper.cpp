#include "videodrawhelper.h"

#include <QDebug>
#include <QWidget>
#include <QKeyEvent>

VideoDrawHelper::VideoDrawHelper(uint32_t sessionID):
  sessionID_(sessionID),
  tmpParent_(nullptr)
{
}

void VideoDrawHelper::keyPressEvent(QWidget* widget, QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    qDebug() << "Esc key pressed";
    if(widget->isFullScreen() && sessionID_ != 0)
    {
      exitFullscreen(widget);
    }
  }
  else
  {
    qDebug() << "You Pressed non-ESC Key";
  }
}

void VideoDrawHelper::mouseDoubleClickEvent(QWidget* widget, QMouseEvent *e) {
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
  qDebug() << "Setting VideoGLWidget fullscreen";

  tmpParent_ = widget->parentWidget();
  widget->setParent(nullptr);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowFullScreen);
  widget->raise();
}


void VideoDrawHelper::exitFullscreen(QWidget* widget)
{
  qDebug() << "Returning GL video widget to original place.";
  widget->setParent(tmpParent_);
  //this->showMaximized();
  widget->show();
  widget->setWindowState(Qt::WindowMaximized);

  emit reattach(sessionID_, widget);
}
