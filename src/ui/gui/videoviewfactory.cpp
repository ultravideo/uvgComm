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
  sessionIDtoWidgetlist_(),
  sessionIDtoVideolist_(),
  opengl_(false)
{}

uint32_t VideoviewFactory::createWidget(uint32_t sessionID, QWidget* parent,
                                        ConferenceView* conf)
{
  qDebug() << "View, VideoFactory : Creating videowidget for sessionID:" << sessionID;
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  int opengl = settings.value("video/opengl").toInt();

  QWidget* vw = nullptr;
  VideoInterface* video = nullptr;

  if(false && !opengl_) // YUV widget not working yet
  {
    VideoYUVWidget* yuv = new VideoYUVWidget(parent, sessionID);
    vw = yuv;
    video = yuv;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface

    // signals reattaching after fullscreen mode
    QObject::connect(yuv, &VideoYUVWidget::reattach, conf, &ConferenceView::reattachWidget);
    QObject::connect(yuv, &VideoYUVWidget::detach, conf, &ConferenceView::detachWidget);

    opengl_ = true;
  }
  else if(opengl == 1)
  {
    VideoGLWidget* opengl = new VideoGLWidget(parent, sessionID);
    vw = opengl;
    video = opengl;

    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface

    // signals reattaching after fullscreen mode
    QObject::connect(opengl, &VideoGLWidget::reattach, conf, &ConferenceView::reattachWidget);
    QObject::connect(opengl, &VideoGLWidget::detach, conf, &ConferenceView::detachWidget);
  }
  else
  {
    VideoWidget* normal = new VideoWidget(parent, sessionID);
    vw = normal;
    video = normal;
    // couldn't get this to work with videointerface, so the videowidget is used.
    // TODO: try qobject_cast to get the signal working for interface

    // signals reattaching after fullscreen mode
    QObject::connect(normal, &VideoWidget::reattach, conf, &ConferenceView::reattachWidget);
    QObject::connect(normal, &VideoWidget::detach, conf, &ConferenceView::detachWidget);
  }

  if (vw != nullptr && video != nullptr)
  {
    checkInitializations(sessionID);
    sessionIDtoWidgetlist_[sessionID]->push_back(vw);
    sessionIDtoVideolist_[sessionID]->push_back(video);
    printDebug(DEBUG_NORMAL, "VideoviewFactory", DC_ADD_MEDIA, "Created video widget.", {"SessionID", "View Number"},
        {QString::number(sessionID), QString::number(sessionIDtoWidgetlist_[sessionID]->size())});
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", DC_ADD_MEDIA, "Failed to create view widget.");
  }

  return sessionIDtoWidgetlist_[sessionID]->size() - 1;
}


uint32_t VideoviewFactory::setSelfview(VideoInterface *video, QWidget *view)
{
  qDebug() << "Setting self view";
  checkInitializations(0);

  sessionIDtoWidgetlist_[0]->push_back(view);
  sessionIDtoVideolist_[0]->push_back(video);
  return sessionIDtoVideolist_[0]->size() - 1;
}


QWidget* VideoviewFactory::getView(uint32_t sessionID, uint32_t viewID)
{
  if(sessionIDtoWidgetlist_.find(sessionID) == sessionIDtoWidgetlist_.end()
     || sessionIDtoWidgetlist_[sessionID]->size() <= viewID)
  {
    Q_ASSERT(false);
    printDebug(DEBUG_PROGRAM_ERROR, "VideoViewFactory", DC_NO_CONTEXT, "Tried to get a video widget that doesn't exists",
      {"SessionID"}, {QString::number(sessionID)});
    return nullptr;
  }
  return sessionIDtoWidgetlist_[sessionID]->at(viewID);
}


VideoInterface* VideoviewFactory::getVideo(uint32_t sessionID, uint32_t videoID)
{
  if(sessionIDtoVideolist_.find(sessionID) == sessionIDtoVideolist_.end()
     || sessionIDtoVideolist_[sessionID]->size() <= videoID)
  {
    Q_ASSERT(false);
    printDebug(DEBUG_PROGRAM_ERROR, "VideoViewFactory", DC_STARTUP,
               "Tried to get a video widget that doesn't exists.",
              {"SessionID"}, {QString::number(sessionID)});
    return nullptr;
  }
  return sessionIDtoVideolist_[sessionID]->at(videoID);
}


void VideoviewFactory::clearWidgets()
{
  printDebug(DEBUG_NORMAL, "VideoviewFactory", DC_END_CALL, "Clearing all widgets.");
  if (sessionIDtoWidgetlist_.size() == sessionIDtoVideolist_.size())
  {
    printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", DC_ADD_MEDIA, "Internal state not correct!");
  }

  for (unsigned int i = sessionIDtoWidgetlist_.size(); i > 0; --i)
  {
    sessionIDtoWidgetlist_.erase(i);
  }

  for (unsigned int i = sessionIDtoVideolist_.size(); i > 0; --i)
  {
    sessionIDtoVideolist_.erase(i);
  }

}


void VideoviewFactory::checkInitializations(uint32_t sessionID)
{
  if(sessionIDtoWidgetlist_.find(sessionID) == sessionIDtoWidgetlist_.end())
  {
    std::shared_ptr<std::vector<QWidget*>> widgets;
    widgets = std::shared_ptr<std::vector<QWidget*>>(new std::vector<QWidget*>);
    sessionIDtoWidgetlist_[sessionID] = widgets;
  }

  if (sessionIDtoVideolist_.find(sessionID) == sessionIDtoVideolist_.end())
  {
    std::shared_ptr<std::vector<VideoInterface*>> videos;
    videos = std::shared_ptr<std::vector<VideoInterface*>>(new std::vector<VideoInterface*>);
    sessionIDtoVideolist_[sessionID] = videos;
  }
}
