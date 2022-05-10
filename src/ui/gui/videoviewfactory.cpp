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

VideoviewFactory::VideoviewFactory(ConferenceView* conf):
  conf_(conf),
  sessionIDtoWidgetlist_(),
  sessionIDtoVideolist_(),
  selfViews_(),
  selfVideos_(),
  opengl_(false)
{}

VideoviewFactory::~VideoviewFactory()
{
  sessionIDtoWidgetlist_.clear(); // these are the same pointers as in videolist
  sessionIDtoVideolist_.clear();
}


void VideoviewFactory::createWidget(uint32_t sessionID)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "View Factory",
                                  "Creating videoWidget", {"SessionID"}, {QString::number(sessionID)});

  bool openGLEnabled = settingEnabled(SettingsKey::videoOpenGL);

  QWidget* vw = nullptr;
  VideoInterface* video = nullptr;


  // couldn't get these to work with videointerface, so the videowidget is used.
  // TODO: try qobject_cast to get the signal working for videointerface

  QWidget* parent = nullptr;

  if(false && !opengl_) // YUV widget not working yet
  {
    VideoYUVWidget* yuv = new VideoYUVWidget(parent, sessionID);
    vw = yuv;
    video = yuv;

    // signals reattaching after fullscreen mode
    QObject::connect(yuv, &VideoYUVWidget::reattach, conf_, &ConferenceView::reattachWidget);
    QObject::connect(yuv, &VideoYUVWidget::detach, conf_, &ConferenceView::detachWidget);

    opengl_ = true;
  }
  else if(openGLEnabled)
  {
    VideoGLWidget* opengl = new VideoGLWidget(parent, sessionID);
    vw = opengl;
    video = opengl;

    // couldn't get this to work with videointerface, so the videowidget is used.

    // signals reattaching after fullscreen mode
    QObject::connect(opengl, &VideoGLWidget::reattach, conf_, &ConferenceView::reattachWidget);
    QObject::connect(opengl, &VideoGLWidget::detach, conf_, &ConferenceView::detachWidget);
  }
  else
  {
    VideoWidget* normal = new VideoWidget(parent, sessionID);
    vw = normal;
    video = normal;
    // couldn't get this to work with videointerface, so the videowidget is used.

    // signals reattaching after fullscreen mode
    QObject::connect(normal, &VideoWidget::reattach, conf_, &ConferenceView::reattachWidget);
    QObject::connect(normal, &VideoWidget::detach, conf_, &ConferenceView::detachWidget);
  }

  if (vw != nullptr && video != nullptr)
  {
    sessionIDtoWidgetlist_[sessionID] = vw;
    sessionIDtoVideolist_[sessionID]  = video;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory", 
                                    "Created video widget.", {"SessionID"},
                                    {QString::number(sessionID)});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", "Failed to create view widget.");
  }
}


void VideoviewFactory::addSelfview(VideoInterface *video, QWidget *view)
{ 
  selfViews_.push_back(view);
  selfVideos_.push_back(video);
}


QList<QWidget*> VideoviewFactory::getSelfViews()
{
  return selfViews_;
}


QList<VideoInterface*> VideoviewFactory::getSelfVideos()
{
  return selfVideos_;
}


QWidget* VideoviewFactory::getView(uint32_t sessionID)
{
  if(sessionIDtoWidgetlist_.find(sessionID) == sessionIDtoWidgetlist_.end())
  {
    createWidget(sessionID);
  }
  return sessionIDtoWidgetlist_[sessionID];
}


VideoInterface* VideoviewFactory::getVideo(uint32_t sessionID)
{
  if(sessionIDtoVideolist_.find(sessionID) == sessionIDtoVideolist_.end())
  {
    createWidget(sessionID);
  }
  return sessionIDtoVideolist_[sessionID];
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
    delete sessionIDtoVideolist_[sessionID];
    sessionIDtoVideolist_.erase(sessionID);
  }
}
