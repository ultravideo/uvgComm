#pragma once



#include "mediastreamer.h"
#include "filter.h"

#include <QWidget>

#include <vector>

class VideoWidget;

class FilterGraph
{
public:
  FilterGraph();

  void constructVideoGraph(VideoWidget *videoWidget);
  void constructAudioGraph();
  void deconstruct();
  void run();
  void stop();


private:

  std::vector<Filter*> filters_;

  Mediastreamer streamControl_;

};
