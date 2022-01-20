#include "videoviewfactory.h"

#include "videowidget.h"
#include "videoglwidget.h"
#include "videoyuvwidget.h"

// this annoys me, but I can live with it. The only smart way to fix it would be to get signal connect working
#include "conferenceview.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

VideoviewFactory::VideoviewFactory():
  sessionIDtoWidgetlist_(),
  sessionIDtoVideolist_(),
  opengl_(false)
{}

size_t VideoviewFactory::createWidget(uint32_t sessionID, QWidget* parent,
                                        ConferenceView* conf)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "View Factory",
                                  "Creating videoWidget", {"SessionID"}, {QString::number(sessionID)});

  bool openGLEnabled = settingEnabled(SettingsKey::videoOpenGL);

  QWidget* vw = nullptr;
  VideoInterface* video = nullptr;


  // couldn't get these to work with videointerface, so the videowidget is used.
  // TODO: try qobject_cast to get the signal working for videointerface

  if(false && !opengl_) // YUV widget not working yet
  {
    VideoYUVWidget* yuv = new VideoYUVWidget(parent, sessionID);
    vw = yuv;
    video = yuv;

    // signals reattaching after fullscreen mode
    QObject::connect(yuv, &VideoYUVWidget::reattach, conf, &ConferenceView::reattachWidget);
    QObject::connect(yuv, &VideoYUVWidget::detach, conf, &ConferenceView::detachWidget);

    opengl_ = true;
  }
  else if(openGLEnabled)
  {
    VideoGLWidget* opengl = new VideoGLWidget(parent, sessionID);
    vw = opengl;
    video = opengl;

    // couldn't get this to work with videointerface, so the videowidget is used.

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

    // signals reattaching after fullscreen mode
    QObject::connect(normal, &VideoWidget::reattach, conf, &ConferenceView::reattachWidget);
    QObject::connect(normal, &VideoWidget::detach, conf, &ConferenceView::detachWidget);
  }

  if (vw != nullptr && video != nullptr)
  {
    checkInitializations(sessionID);
    sessionIDtoWidgetlist_[sessionID]->push_back(vw);
    sessionIDtoVideolist_[sessionID]->push_back(video);
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory", 
                                    "Created video widget.", {"SessionID", "View Number"},
                                    {QString::number(sessionID), QString::number(sessionIDtoWidgetlist_[sessionID]->size())});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", "Failed to create view widget.");
  }

  return sessionIDtoWidgetlist_[sessionID]->size() - 1;
}


size_t VideoviewFactory::setSelfview(VideoInterface *video, QWidget *view)
{ 
  checkInitializations(0);

  sessionIDtoWidgetlist_[0]->push_back(view);
  sessionIDtoVideolist_[0]->push_back(video);
  return sessionIDtoVideolist_[0]->size() - 1;
}


QWidget* VideoviewFactory::getView(uint32_t sessionID, size_t viewID)
{
  if(sessionIDtoWidgetlist_.find(sessionID) == sessionIDtoWidgetlist_.end()
     || sessionIDtoWidgetlist_[sessionID]->size() <= viewID)
  {
    Q_ASSERT(false);
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoViewFactory", 
                                    "Tried to get a video widget that doesn't exists",
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
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoViewFactory", 
                                    "Tried to get a video widget that doesn't exists.",
                                    {"SessionID"}, {QString::number(sessionID)});
    return nullptr;
  }
  return sessionIDtoVideolist_[sessionID]->at(videoID);
}


void VideoviewFactory::clearWidgets(uint32_t sessionID)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",  "Clearing widgets",
                                  {"SessionID"}, {QString::number(sessionID)});

  if (sessionIDtoWidgetlist_.find(sessionID) != sessionIDtoWidgetlist_.end())
  {
    sessionIDtoWidgetlist_.erase(sessionID);
  }

  if (sessionIDtoVideolist_.find(sessionID) != sessionIDtoVideolist_.end())
  {
    sessionIDtoVideolist_.erase(sessionID);
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
