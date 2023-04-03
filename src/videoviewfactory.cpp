#include "videoviewfactory.h"

#include "ui/gui/videowidget.h"


#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

VideoviewFactory::VideoviewFactory():
  sessionIDtoWidgetlist_(),
  sessionIDtoVideolist_(),
  selfViews_(),
  opengl_(false)
{}


VideoviewFactory::~VideoviewFactory()
{
  sessionIDtoWidgetlist_.clear(); // these are the same pointers as in videolist
  sessionIDtoVideolist_.clear();
}


void VideoviewFactory::addSelfview(VideoWidget *view)
{ 
  selfViews_.push_back(view);
}


QList<QWidget*> VideoviewFactory::getSelfViews()
{
  QList<QWidget*> views;

  for (auto& view : selfViews_)
  {
    views.push_back(view);
  }

  return views;
}


QList<VideoInterface*> VideoviewFactory::getSelfVideos()
{
  QList<VideoInterface*> videos;

  for (auto& view : selfViews_)
  {
    videos.push_back(view);
  }

  return videos;
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

  QWidget* widget = nullptr;

  if (sessionIDtoWidgetlist_.find(sessionID) != sessionIDtoWidgetlist_.end())
  {
    widget = sessionIDtoWidgetlist_.at(sessionID);
    sessionIDtoWidgetlist_.erase(sessionID);
  }

  if (sessionIDtoVideolist_.find(sessionID) != sessionIDtoVideolist_.end())
  {
    sessionIDtoVideolist_.erase(sessionID);
  }

  delete widget;
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

    VideoWidget* normal = new VideoWidget(parent, sessionID);
    vw = normal;
    video = normal;

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
