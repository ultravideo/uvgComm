#pragma once



#include "rtpstreamer.h"
#include "filter.h"

#include <QWidget>

#include <vector>

class VideoWidget;

class FilterGraph
{
public:
  FilterGraph();

  void constructVideoGraph(VideoWidget *selfView, VideoWidget *videoCall,
                           in_addr ip, uint16_t port);

  void constructAudioGraph();
  void deconstruct();
  void run();
  void stop();


private:

  std::vector<Filter*> filters_;

  RTPStreamer streamer_;

};
