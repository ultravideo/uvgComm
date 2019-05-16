#include "videoviewfactory.h"

#include "videowidget.h"
#include "videoglwidget.h"
#include "videoyuvwidget.h"

// this annoys me, but I can live with it. The only smart way to fix it would be to get signal connect working
#include "conferenceview.h"

#include "common.h"

#include <QSettings>
#include <QDebug>

VideoviewFactory::VideoviewFactory():
  widgets_(),
  videos_(),
  opengl_(false)
{}

void VideoviewFactory::createWidget(uint32_t sessionID, QWidget* parent, ConferenceView* conf)
{
  qDebug() << "View, VideoFactory : Creating videowidget for sessionID:" << sessionID;

  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  int opengl = settings.value("video/opengl").toInt();

  if(false && !opengl_)
  {
    VideoYUVWidget* vw = new VideoYUVWidget(parent, sessionID);
    widgets_[sessionID] = vw;
    videos_[sessionID] = vw;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface

    // signals reattaching after fullscreen mode
    QObject::connect(vw, &VideoYUVWidget::reattach, conf, &ConferenceView::attachWidget);
    QObject::connect(vw, &VideoYUVWidget::detach, conf, &ConferenceView::detachWidget);

    opengl_ = true;
  }
  else if(opengl == 1)
  {
    VideoGLWidget* vw = new VideoGLWidget(parent, sessionID);
    widgets_[sessionID] = vw;
    videos_[sessionID] = vw;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface

    // signals reattaching after fullscreen mode
    QObject::connect(vw, &VideoGLWidget::reattach, conf, &ConferenceView::attachWidget);
    QObject::connect(vw, &VideoGLWidget::detach, conf, &ConferenceView::detachWidget);
  }
  else
  {
    VideoWidget* vw = new VideoWidget(parent, sessionID);
    widgets_[sessionID] = vw;
    videos_[sessionID] = vw;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface

    // signals reattaching after fullscreen mode
    QObject::connect(vw, &VideoWidget::reattach, conf, &ConferenceView::attachWidget);
    QObject::connect(vw, &VideoWidget::detach, conf, &ConferenceView::detachWidget);
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
    printDebug(DEBUG_ERROR, "VideoViewFactory", "Tried to get a video widget that doesn't exists",
      {"SessionID"}, {QString::number(sessionID)});
    return nullptr;
  }
  return widgets_[sessionID];
}

VideoInterface* VideoviewFactory::getVideo(uint32_t sessionID)
{
  if(videos_.find(sessionID) == videos_.end())
  {
    printDebug(DEBUG_ERROR, "VideoViewFactory", "Initiate",
               "Tried to get a video widget that doesn't exists.",
              {"SessionID"}, {QString::number(sessionID)});
    return nullptr;
  }
  return videos_[sessionID];
}
