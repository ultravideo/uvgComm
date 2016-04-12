#pragma once

#include <vector>
#include "filter.h"





class FilterGraph
{
public:
  FilterGraph();

  void constructVideoGraph();
  void constructAudioGraph();
  void deconstruct();
  void run();
  void stop();


private:

  std::vector<Filter*> filters_;

};
