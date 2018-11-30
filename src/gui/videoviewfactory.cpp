#include "videoviewfactory.h"

#include "gui/videowidget.h"
#include "gui/videoglwidget.h"
#include "gui/videoyuvwidget.h"

// this annoys me, but I can live with it. The only smart way to fix it would be to get signal connect working
#include "conferenceview.h"

#include <QSettings>
#include <QDebug>

VideoviewFactory::VideoviewFactory():
  widgets_(),
  videos_()
{}

void VideoviewFactory::createWidget(uint32_t sessionID, QWidget* parent, ConferenceView* conf)
{
  qDebug() << "Creating videowidget for sessionID:" << sessionID;

  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  int opengl = settings.value("video/opengl").toInt();

  if(opengl == 1)
  {
    VideoYUVWidget* vw = new VideoYUVWidget(parent, sessionID);
    widgets_[sessionID] = vw;
    videos_[sessionID] = vw;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface
    QObject::connect(vw, &VideoYUVWidget::reattach, conf, &ConferenceView::attachWidget);
  }
  else if(false)
  {
    VideoGLWidget* vw = new VideoGLWidget(parent, sessionID);
    widgets_[sessionID] = vw;
    videos_[sessionID] = vw;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface
    QObject::connect(vw, &VideoGLWidget::reattach, conf, &ConferenceView::attachWidget);
  }
  else
  {
    VideoWidget* vw = new VideoWidget(parent, sessionID);
    widgets_[sessionID] = vw;
    videos_[sessionID] = vw;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface
    QObject::connect(vw, &VideoWidget::reattach, conf, &ConferenceView::attachWidget);
  }
}

void VideoviewFactory::setSelfview(VideoInterface *video, QWidget *view)
{
  widgets_[0] = view;
  videos_[0] = video;
}

QWidget* VideoviewFactory::getView(uint32_t sessionID)
{
  if(widgets_.find(sessionID) == widgets_.end())
  {
    qWarning() << "ERROR: Tried to get a video widget that doesn't exists:" << sessionID;
    return nullptr;
  }
  return widgets_[sessionID];
}

VideoInterface* VideoviewFactory::getVideo(uint32_t sessionID)
{
  if(videos_.find(sessionID) == videos_.end())
  {
    qWarning() << "ERROR: Tried to get a video widget that doesn't exists:" << sessionID;
    return nullptr;
  }
  return videos_[sessionID];
}
