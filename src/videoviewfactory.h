#pragma once

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

  // id is the index of that view or video
  QWidget*        getView  (uint32_t videoID);
  VideoInterface* getVideo (uint32_t videoID);

  QList<QWidget*>        getSelfViews();
  QList<VideoInterface*> getSelfVideos();

  // Does not clear selfview
  void clearWidgets(uint32_t videoID);

private:

  // conferenceview is needed for connecting reattach signal, because I couldn't get the
  // the interface signal connected for some reason.
  void createWidget(uint32_t videoID);

  std::map<uint32_t, QWidget*>        videoIDtoWidgetlist_;
  std::map<uint32_t, VideoInterface*> videoIDtoVideolist_;

  QList<VideoWidget*> selfViews_;

  bool opengl_;
};

