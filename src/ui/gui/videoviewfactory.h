#pragma once

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
  uint32_t createWidget(uint32_t sessionID, QWidget* parent, ConferenceView* conf);

  // set self view which is part of the
  // returns viewID which is also videoID
  uint32_t setSelfview(VideoInterface* video, QWidget* view);

  // 0 in sessionID is for selfview
  // id is the index of that view or video
  QWidget* getView(uint32_t sessionID, uint32_t viewID);
  VideoInterface* getVideo(uint32_t sessionID, uint32_t videoID);

  // Does not clear selfview
  void clearWidgets();

private:

  void checkInitializations(uint32_t sessionID);

  //TODO: make shared ptr so they get deleted
  std::map<uint32_t, std::shared_ptr<std::vector<QWidget*>>> sessionIDtoWidgetlist_;
  std::map<uint32_t, std::shared_ptr<std::vector<VideoInterface*>>> sessionIDtoVideolist_;

  bool opengl_;
};
