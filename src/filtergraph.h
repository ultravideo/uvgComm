#pragma once

#include "rtpstreamer.h"
#include "filter.h"

#include <QWidget>
#include <QAudioFormat>

#include <vector>

class VideoWidget;
class StatisticsInterface;
class AudioOutput;

typedef int16_t ParticipantID;

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

  void removeParticipant(ParticipantID peer);

  void restart();
  void stop();

  void uninit();

private:

  // iniates camera and attaches a self view to it.
  void initSelfView(VideoWidget *selfView, QSize resolution);

  // iniates encoder and attaches it
  void initVideoSend(QSize resolution);

  // iniates encoder and attaches it
  void initAudioSend();

  struct Peer
  {
    Filter* audioFramedSource; // sends audio
    Filter* videoFramedSource; // sends video

    std::vector<Filter*> videoReceive;
    std::vector<Filter*> audioReceive;

    AudioOutput* output;

    PeerID streamID;
  };

  void destroyPeer(Peer* peer);

  void destroyFilters(std::vector<Filter*>& filters);

  void deconstruct();

  // These functions are used to manipulate filter graphs
  void sendVideoto(Peer* send, uint16_t port);
  void receiveVideoFrom(Peer* recv, uint16_t port, VideoWidget *view);
  void sendAudioTo(Peer* send, uint16_t port);
  void receiveAudioFrom(Peer* recv, uint16_t port);

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
