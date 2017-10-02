#pragma once

#include <QWidget>
#include <QAudioFormat>

#include <vector>
#include <memory>

class VideoWidget;
class StatisticsInterface;
class AudioOutput;
class Filter;


class FilterGraph
{
public:
  FilterGraph();

  void init(VideoWidget* selfView, StatisticsInterface *stats);
  void uninit();

  // These functions are used to manipulate filter graphs regarding a peer
  void sendVideoto(int16_t id, std::shared_ptr<Filter> videoFramedSource);
  void receiveVideoFrom(int16_t id, std::shared_ptr<Filter> videoSink, VideoWidget *view);
  void sendAudioTo(int16_t id, std::shared_ptr<Filter> audioFramedSource);
  void receiveAudioFrom(int16_t id, std::shared_ptr<Filter> audioSink);

  void removeParticipant(int16_t id);

  void mic(bool state);
  void camera(bool state);
  void running(bool state);

  void print();

  void updateSettings();

private:

  // adds fitler to graph and connects it to connectIndex unless this is the first filter in graph.
  // adds format conversion if needed.
  bool addToGraph(std::shared_ptr<Filter> filter,
                  std::vector<std::shared_ptr<Filter>>& graph,
                  unsigned int connectIndex = 0);

  // connects the two filters and checks for any problems
  bool connectFilters(std::shared_ptr<Filter> filter, std::shared_ptr<Filter> previous);

  // makes sure the participant exists and adds if necessary
  void checkParticipant(int16_t id);

  // iniates camera and attaches a self view to it.
  void initSelfView(VideoWidget *selfView);

  // iniates encoder and attaches it
  void initVideoSend();

  // iniates encoder and attaches it
  void initAudioSend();

  struct Peer
  {
    std::shared_ptr<Filter> audioFramedSource; // sends audio
    std::shared_ptr<Filter> videoFramedSource; // sends video

    std::vector<std::shared_ptr<Filter>> videoReceive;
    std::vector<std::shared_ptr<Filter>> audioReceive;

    AudioOutput* output; // plays audio coming from this peer
  };

  void destroyPeer(Peer* peer);

  void destroyFilters(std::vector<std::shared_ptr<Filter>>& filters);

  void deconstruct();

  unsigned int camera_;
  unsigned int kvazaar_;
  unsigned int opusEncoder_;

  // id is also the index of the Peer in this vector
  std::vector<Peer*> peers_;

  std::vector<std::shared_ptr<Filter>> videoSend_;
  std::vector<std::shared_ptr<Filter>> audioSend_;

  VideoWidget *selfView_;

  StatisticsInterface* stats_;

  // audio configs
  QAudioFormat format_;

  QString videoFormat_;
};
