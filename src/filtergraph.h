#pragma once

#include <QWidget>
#include <QAudioFormat>

#include <vector>

class VideoWidget;
class StatisticsInterface;
class AudioOutput;
class Filter;


class FilterGraph
{
public:
  FilterGraph(StatisticsInterface *stats);

  void init(VideoWidget* selfView, QSize resolution);
  void uninit();

  // These functions are used to manipulate filter graphs regarding a peer
  void sendVideoto(int16_t id, Filter* videoFramedSource);
  void receiveVideoFrom(int16_t id, Filter* videoSink, VideoWidget *view);
  void sendAudioTo(int16_t id, Filter* audioFramedSource);
  void receiveAudioFrom(int16_t id, Filter* audioSink);

  void removeParticipant(int16_t id);

  void mic(bool state);
  void camera(bool state);
  void running(bool state);

  void print();

private:

  // makes sure the participant exists and adds if necessary
  void checkParticipant(int16_t id);

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

    Filter* audioSink; // receives audio
    Filter* videoSink; // receives video

    std::vector<Filter*> videoReceive;
    std::vector<Filter*> audioReceive;

    AudioOutput* output; // plays audio coming from this peer
  };

  void destroyPeer(Peer* peer);

  void destroyFilters(std::vector<Filter*>& filters);

  void deconstruct();

  // id is also the index of the Peer in this vector
  std::vector<Peer*> peers_;

  std::vector<Filter*> videoSend_;
  std::vector<Filter*> audioSend_;

  VideoWidget *selfView_;

  StatisticsInterface* stats_;

  //config stuff, moved to config later
  uint16_t frameRate_;
  QSize resolution_;

  // audio configs
  QAudioFormat format_;
};
