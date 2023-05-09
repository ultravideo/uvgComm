#include "videoviewfactory.h"

#include "ui/gui/videowidget.h"


#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

VideoviewFactory::VideoviewFactory():
  videoIDtoWidgetlist_(),
  videoIDtoVideolist_(),
  selfViews_(),
  opengl_(false)
{}


VideoviewFactory::~VideoviewFactory()
{
  videoIDtoWidgetlist_.clear(); // these are the same pointers as in videolist
  videoIDtoVideolist_.clear();
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


QWidget* VideoviewFactory::getView(uint32_t videoID)
{
  if(videoIDtoWidgetlist_.find(videoID) == videoIDtoWidgetlist_.end())
  {
    createWidget(videoID);
  }
  return videoIDtoWidgetlist_[videoID];
}


VideoInterface* VideoviewFactory::getVideo(uint32_t videoID)
{
  if(videoIDtoVideolist_.find(videoID) == videoIDtoVideolist_.end())
  {
    createWidget(videoID);
  }
  return videoIDtoVideolist_[videoID];
}


void VideoviewFactory::clearWidgets(uint32_t videoID)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",  "Clearing widgets",
                                  {"videoID"}, {QString::number(videoID)});

  QWidget* widget = nullptr;

  if (videoIDtoWidgetlist_.find(videoID) != videoIDtoWidgetlist_.end())
  {
    widget = videoIDtoWidgetlist_.at(videoID);
    videoIDtoWidgetlist_.erase(videoID);
  }

  if (videoIDtoVideolist_.find(videoID) != videoIDtoVideolist_.end())
  {
    videoIDtoVideolist_.erase(videoID);
  }

  delete widget;
}


void VideoviewFactory::createWidget(uint32_t videoID)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "View Factory",
                                  "Creating videoWidget", {"videoID"}, {QString::number(videoID)});

  bool openGLEnabled = settingEnabled(SettingsKey::videoOpenGL);

  QWidget* vw = nullptr;
  VideoInterface* video = nullptr;


  // couldn't get these to work with videointerface, so the videowidget is used.
  // TODO: try qobject_cast to get the signal working for videointerface

  QWidget* parent = nullptr;

    VideoWidget* normal = new VideoWidget(parent, videoID);
    vw = normal;
    video = normal;

  if (vw != nullptr && video != nullptr)
  {
    videoIDtoWidgetlist_[videoID] = vw;
    videoIDtoVideolist_[videoID]  = video;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",
                                    "Created video widget.", {"videoID"},
                                    {QString::number(videoID)});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", "Failed to create view widget.");
  }
}
