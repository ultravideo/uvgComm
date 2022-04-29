#pragma once

#include <QList>

#include <stdint.h>

#include <map>
#include <vector>
#include <memory>

// handles creating and managing the view components.

class QWidget;
class VideoInterface;
class ConferenceView;

class VideoviewFactory
{
public:
  VideoviewFactory();

  // conferenceview is needed for connecting reattach signal, because I couldn't get the
  // the interface signal connected for some reason.
  // returns viewID which is also videoID
  size_t createWidget(uint32_t sessionID, QWidget* parent, ConferenceView* conf);

  // set self view
  void addSelfview(VideoInterface* video, QWidget* view);

  // id is the index of that view or video
  QWidget* getView(uint32_t sessionID, size_t viewID);
  VideoInterface* getVideo(uint32_t sessionID, uint32_t videoID);

  QList<QWidget*> getSelfViews();
  QList<VideoInterface*> getSelfVideos();

  // Does not clear selfview
  void clearWidgets(uint32_t sessionID);

private:

  void checkInitializations(uint32_t sessionID);

  // TODO: make these are deleted at some point
  std::map<uint32_t, std::shared_ptr<std::vector<QWidget*>>> sessionIDtoWidgetlist_;
  std::map<uint32_t, std::shared_ptr<std::vector<VideoInterface*>>> sessionIDtoVideolist_;

  QList<QWidget*> selfViews_;
  QList<VideoInterface*> selfVideos_;

  bool opengl_;
};

