#include "videoviewfactory.h"

#include "ui/gui/videowidget.h"


#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

VideoviewFactory::VideoviewFactory():
    mediaIDtoWidgetlist_(),
    mediaIDtoVideolist_(),
  selfViews_(),
  opengl_(false)
{}


VideoviewFactory::~VideoviewFactory()
{
    mediaIDtoWidgetlist_.clear(); // these are the same pointers as in videolist
    mediaIDtoVideolist_.clear();
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


QWidget* VideoviewFactory::getView(MediaID& id)
{
  return mediaIDtoWidgetlist_[id];
}


VideoInterface* VideoviewFactory::getVideo(const MediaID &id)
{
  return mediaIDtoVideolist_[id];
}


void VideoviewFactory::clearWidgets(MediaID &id)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",  "Clearing widgets",
                                  {"videoID"}, {id.toString()});

  QWidget* widget = nullptr;

  if (mediaIDtoWidgetlist_.find(id) != mediaIDtoWidgetlist_.end())
  {
    widget = mediaIDtoWidgetlist_.at(id);
    mediaIDtoWidgetlist_.erase(id);
  }

  if (mediaIDtoVideolist_.find(id) != mediaIDtoVideolist_.end())
  {
    mediaIDtoVideolist_.erase(id);
  }

  delete widget;
}


void VideoviewFactory::createWidget(uint32_t sessionID, LayoutID layoutID, const MediaID& id)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "View Factory",
                                  "Creating videoWidget", {"videoID"}, {id.toString()});

  bool openGLEnabled = settingEnabled(SettingsKey::videoOpenGL);

  QWidget* vw = nullptr;
  VideoInterface* video = nullptr;

  // couldn't get these to work with videointerface, so the videowidget is used.
  // TODO: try qobject_cast to get the signal working for videointerface

  QWidget* parent = nullptr;

    VideoWidget* normal = new VideoWidget(parent, sessionID, layoutID);
    vw = normal;
    video = normal;

  if (vw != nullptr && video != nullptr)
  {
    mediaIDtoWidgetlist_[id] = vw;
    mediaIDtoVideolist_[id]  = video;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",
                                    "Created video widget.", {"videoID"},
                                    {id.toString()});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", "Failed to create view widget.");
  }
}
