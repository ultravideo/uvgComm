#pragma once

#include "rtpstreamer.h"
#include "filter.h"

#include <QWidget>

#include <vector>

class VideoWidget;

typedef uint16_t ParticipantID;

class FilterGraph
{
public:
  FilterGraph();

  void init(VideoWidget* selfView, QSize resolution);

  ParticipantID addParticipant(in_addr ip, uint16_t port, VideoWidget* view = NULL,
                      bool wantsAudio = true, bool sendsAudio = true,
                      bool wantsVideo = true, bool sendsVideo = true);

  void modifyParticipant(ParticipantID peer, VideoWidget* view = NULL,
                         bool wantsAudio = true, bool sendsAudio = true,
                         bool wantsVideo = true, bool sendsVideo = true);

  // NON-FUNCTIONAL at the moment
  void removeParticipant(ParticipantID peer);

  void restart();
  void stop();

  void uninit();

private:

  // starts the camera and the encoder
  void initSender(VideoWidget *selfView, QSize resolution);

  // attaches an RTP destination to video graph
  void attachVideoDestination(in_addr ip, uint16_t port);

  void attachVideoSource(in_addr ip, uint16_t port,
                         VideoWidget *participantView);

  void attachAudioDestination();

  void attachAudioSource();

  void deconstruct();

  std::vector<Filter*> filters_;

  bool videoSendIniated_;
  unsigned int encoderFilter_;

  RTPStreamer streamer_;

};
