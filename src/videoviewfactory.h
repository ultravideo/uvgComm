#pragma once

#include "global.h"

#include <QList>

#include <stdint.h>

#include <map>


/* Handles creating and managing the video view components.
 * Is responsible for releasing the memory.
 */

class QWidget;
class VideoInterface;
class VideoWidget;

class VideoviewFactory
{
public:
  VideoviewFactory();
  ~VideoviewFactory();

  // set self view
  void addSelfview(VideoWidget* view);

  void createWidget(uint32_t sessionID, LayoutID layoutID, const std::set<uint32_t>& remoteSSRCs);

  // id is the index of that view or video
  QWidget*        getView  (uint32_t remoteSSRC);
  VideoInterface* getVideo (const uint32_t remoteSSRC);

  QList<QWidget*>        getSelfViews();
  QList<VideoInterface*> getSelfVideos();

  // Does not clear selfview
  void clearWidgets(uint32_t remoteSSRC);

  int getConferenceSize() const
  {
    return viewIDToWidgetlist_.size();
  }

private:

  std::map<uint32_t, uint32_t> ssrcToViewID_;

  std::map<uint32_t, QWidget*>        viewIDToWidgetlist_;
  std::map<uint32_t, VideoInterface*> viewIDToVideolist_;

  QList<VideoWidget*> selfViews_;

  bool opengl_;

  uint32_t nextViewID_;
};

