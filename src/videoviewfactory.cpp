#include "videoviewfactory.h"

#include "ui/gui/videowidget.h"


#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

VideoviewFactory::VideoviewFactory():
    viewIDToWidgetlist_(),
    viewIDToVideolist_(),
  selfViews_(),
  opengl_(false),
    nextViewID_(1)
{}


VideoviewFactory::~VideoviewFactory()
{
  viewIDToWidgetlist_.clear(); // these are the same pointers as in videolist
  viewIDToVideolist_.clear();
  ssrcToViewID_.clear();
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
  if (ssrcToViewID_.find(remoteSSRC) == ssrcToViewID_.end())
  {
    Logger::getLogger()->printProgramError("VideoviewFactory",
                                    "Failed to find widget", {"Remote SSRC"},
                                    {QString::number(remoteSSRC)});
    return nullptr;
  }

  return viewIDToWidgetlist_[ssrcToViewID_[remoteSSRC]];
}


VideoInterface* VideoviewFactory::getVideo(const uint32_t remoteSSRC)
{
  if (ssrcToViewID_.find(remoteSSRC) == ssrcToViewID_.end())
  {
    Logger::getLogger()->printProgramError("VideoviewFactory",
                                    "Failed to find video interface", {"Remote SSRC"},
                                    {QString::number(remoteSSRC)});
    return nullptr;
  }

  return viewIDToVideolist_[ssrcToViewID_[remoteSSRC]];
}


void VideoviewFactory::clearWidgets(uint32_t remoteSSRC)
{
  Logger::getLogger()->printNormal("VideoviewFactory",  "Clearing widgets",
                                  {"videoID"}, {QString::number(remoteSSRC)});

  QWidget* widget = nullptr;

  bool multipleReferences = false;
  if (ssrcToViewID_.find(remoteSSRC) != ssrcToViewID_.end())
  {
    uint32_t viewID = ssrcToViewID_[remoteSSRC];

    for (auto& otherViewID : ssrcToViewID_)
    {
      if (otherViewID.second == viewID &&
          otherViewID.first != remoteSSRC)
      {
        multipleReferences = true;
        break;
      }
    }

    ssrcToViewID_.erase(remoteSSRC);

    // if this was the last reference to the widgets, we can delete them
    if (!multipleReferences)
    {
      if (viewIDToWidgetlist_.find(viewID) != viewIDToWidgetlist_.end())
      {
        widget = viewIDToWidgetlist_.at(viewID);
        viewIDToWidgetlist_.erase(viewID);
      }

      if (viewIDToVideolist_.find(viewID) != viewIDToVideolist_.end())
      {
        viewIDToVideolist_.erase(viewID);
      }
    }
  }



  delete widget;
}


void VideoviewFactory::createWidget(uint32_t sessionID, LayoutID layoutID, const std::set<uint32_t>& remoteSSRCs)
{
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
    uint32_t viewID = nextViewID_;
    nextViewID_++;

    viewIDToWidgetlist_[viewID] = vw;
    viewIDToVideolist_[viewID]  = video;

    for (const auto& remoteSSRC : remoteSSRCs)
    {
      ssrcToViewID_[remoteSSRC] = viewID;
    }

    Logger::getLogger()->printNormal("VideoviewFactory",
                                    "Created video widget.", {"Remote SSRCs", "ViewID"},
                                    {QString::number(remoteSSRCs.size()), QString::number(viewID)});
  }
  else
  {
    Logger::getLogger()->printProgramError("VideoviewFactory", "Failed to create view widget.");
  }
}
