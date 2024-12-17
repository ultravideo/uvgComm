#include "videoviewfactory.h"

#include "ui/gui/videowidget.h"


#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

VideoviewFactory::VideoviewFactory():
    ssrcToWidgetlist_(),
    ssrcToVideolist_(),
  selfViews_(),
  opengl_(false)
{}


VideoviewFactory::~VideoviewFactory()
{
    ssrcToWidgetlist_.clear(); // these are the same pointers as in videolist
    ssrcToVideolist_.clear();
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


QWidget* VideoviewFactory::getView(uint32_t remoteSSRC)
{
  if (ssrcToWidgetlist_.find(remoteSSRC) == ssrcToWidgetlist_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory",
                                    "Failed to find widget", {"Remote SSRC"},
                                    {QString::number(remoteSSRC)});
    return nullptr;
  }

  return ssrcToWidgetlist_[remoteSSRC];
}


VideoInterface* VideoviewFactory::getVideo(const uint32_t remoteSSRC)
{
  if (ssrcToVideolist_.find(remoteSSRC) == ssrcToVideolist_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory",
                                    "Failed to find video interface", {"Remote SSRC"},
                                    {QString::number(remoteSSRC)});
    return nullptr;
  }

  return ssrcToVideolist_[remoteSSRC];
}


void VideoviewFactory::clearWidgets(uint32_t remoteSSRC)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",  "Clearing widgets",
                                  {"videoID"}, {QString::number(remoteSSRC)});

  QWidget* widget = nullptr;

  if (ssrcToWidgetlist_.find(remoteSSRC) != ssrcToWidgetlist_.end())
  {
    widget = ssrcToWidgetlist_.at(remoteSSRC);
    ssrcToWidgetlist_.erase(remoteSSRC);
  }

  if (ssrcToVideolist_.find(remoteSSRC) != ssrcToVideolist_.end())
  {
    ssrcToVideolist_.erase(remoteSSRC);
  }

  delete widget;
}


void VideoviewFactory::createWidget(uint32_t sessionID, LayoutID layoutID, const uint32_t remoteSSRC)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "View Factory",
                                  "Creating videoWidget", {"Remote SSRC"}, {QString::number(remoteSSRC)});

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
    ssrcToWidgetlist_[remoteSSRC] = vw;
    ssrcToVideolist_[remoteSSRC]  = video;
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "VideoviewFactory",
                                    "Created video widget.", {"Remote SSRC"},
                                    {QString::number(remoteSSRC)});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "VideoviewFactory", "Failed to create view widget.");
  }
}
