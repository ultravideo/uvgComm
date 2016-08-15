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

  // starts the camera and the encoder
  void initVideoGraph(VideoWidget *selfView);

  // attaches an RTP destination to video graph
  void attachVideoDestination(in_addr ip, uint16_t port);


  void attachVideoSource(in_addr ip, uint16_t port,
                         VideoWidget *participantView);

  void constructAudioGraph();
  void deconstruct();
  void run();
  void stop();


private:

  std::vector<Filter*> filters_;

  unsigned int senderFilter_;

  RTPStreamer streamer_;

};
