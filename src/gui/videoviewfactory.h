#pragma once

#include <stdint.h>

#include <map>

class QWidget;
class VideoInterface;
class VideoWidget;

class VideoviewFactory
{
public:
  VideoviewFactory();

  VideoWidget* createWidget(uint32_t sessionID, QWidget* parent);

  void setSelfview(VideoWidget* widget);

  // 0 is for selfview
  QWidget* getView(uint32_t sessionID);

  VideoInterface* getVideo(uint32_t sessionID);

private:

  std::map<uint32_t, QWidget*> widgets_;
  std::map<uint32_t, VideoInterface*> videos_;
};
