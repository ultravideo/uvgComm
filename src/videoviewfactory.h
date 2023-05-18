#pragma once

#include "global.h"
#include "mediaid.h"

#include <QList>

#include <stdint.h>

#include <map>
#include <vector>
#include <memory>

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

  void createWidget(uint32_t sessionID, LayoutID layoutID, const MediaID &id);

  // id is the index of that view or video
  QWidget*        getView  (MediaID& id);
  VideoInterface* getVideo (const MediaID &id);

  QList<QWidget*>        getSelfViews();
  QList<VideoInterface*> getSelfVideos();

  // Does not clear selfview
  void clearWidgets(MediaID& id);

private:

  std::map<MediaID, QWidget*>        mediaIDtoWidgetlist_;
  std::map<MediaID, VideoInterface*> mediaIDtoVideolist_;

  QList<VideoWidget*> selfViews_;

  bool opengl_;
};

