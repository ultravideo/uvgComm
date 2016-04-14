#pragma once

#include <vector>
#include "filter.h"

#include <QWidget>

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

};
