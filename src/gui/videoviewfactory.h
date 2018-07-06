#pragma once

#include <stdint.h>

#include <map>

class QWidget;
class VideoInterface;
class ConferenceView;

class VideoviewFactory
{
public:
  VideoviewFactory();

  // conferenceview is needed for connecting reattach signal, because I couldn't get the
  // the interface signal connected for some reason.
  void createWidget(uint32_t sessionID, QWidget* parent, ConferenceView* conf);

  void setSelfview(VideoInterface* video, QWidget* view);

  // 0 is for selfview
  QWidget* getView(uint32_t sessionID);

  VideoInterface* getVideo(uint32_t sessionID);

private:

  std::map<uint32_t, QWidget*> widgets_;
  std::map<uint32_t, VideoInterface*> videos_;
};
