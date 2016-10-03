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

  // attaches filter to the end of the graph and starts it
  void addFilter(Filter* filter, std::vector<Filter*>& graph);

  // iniates camera and attaches a self view to it.
  void initSelfView(VideoWidget *selfView, QSize resolution);

  // iniates encoder and attaches it
  void initVideoSend(QSize resolution);

  // iniates encoder and attaches it
  void initAudioSend();

  // starts the camera and the encoder
  //void initSender(VideoWidget *selfView, QSize resolution);

  struct Peer
  {
    Filter* audioFramedSource; // sends audio
    Filter* videoFramedSource; // sends video

    std::vector<Filter*> videoReceive;
    std::vector<Filter*> audioReceive;

    AudioOutput* output;

    PeerID id;
  };

  // attaches an RTP destination to video graph
  void attachVideoDestination(Peer* recv, in_addr ip, uint16_t port);

  void attachVideoSource(Peer* recv, in_addr ip, uint16_t port,
                         VideoWidget *participantView);

  void attachAudioDestination(Peer* recv);

  void attachAudioSource(Peer* recv);

  void destroyFilters(std::vector<Filter*>& filters);

  void deconstruct();

  std::vector<Peer*> peers_;

  std::vector<Filter*> videoSend_;
  std::vector<Filter*> audioSend_;

  VideoWidget *selfView_;

  RTPStreamer streamer_;

  StatisticsInterface* stats_;

  //config stuff, moved to config later
  uint16_t frameRate_;
  QSize resolution_;

  // audio configs
  QAudioFormat format_;
};
