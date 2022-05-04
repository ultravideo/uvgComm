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
  VideoviewFactory(ConferenceView* conf);
  ~VideoviewFactory();

  // set self view
  void addSelfview(VideoInterface* video, QWidget* view);

  // id is the index of that view or video
  QWidget*        getView  (uint32_t sessionID);
  VideoInterface* getVideo (uint32_t sessionID);

  QList<QWidget*>        getSelfViews();
  QList<VideoInterface*> getSelfVideos();

  // Does not clear selfview
  void clearWidgets(uint32_t sessionID);

private:

  // conferenceview is needed for connecting reattach signal, because I couldn't get the
  // the interface signal connected for some reason.
  void createWidget(uint32_t sessionID);

  ConferenceView* conf_;

  std::map<uint32_t, QWidget*>        sessionIDtoWidgetlist_;
  std::map<uint32_t, VideoInterface*> sessionIDtoVideolist_;

  QList<QWidget*> selfViews_;
  QList<VideoInterface*> selfVideos_;

  bool opengl_;
};

