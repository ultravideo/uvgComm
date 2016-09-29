#pragma once

#include "rtpstreamer.h"
#include "filter.h"

#include <QWidget>
#include <QAudioFormat>

#include <vector>

class VideoWidget;
class StatisticsInterface;
class AudioOutput;

typedef uint16_t ParticipantID;

class FilterGraph
{
public:
  FilterGraph(StatisticsInterface *stats);

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
  std::vector<AudioOutput*> outputs_;

  bool senderIniated_;
  unsigned int videoEncoderFilter_;
  unsigned int audioEncoderFilter_;

  RTPStreamer streamer_;

  StatisticsInterface* stats_;

  //config stuff, moved to config later
  uint16_t frameRate_;
  QSize resolution_;
  VideoWidget *selfView_;

  // audio configs
  QAudioFormat format_;

};
