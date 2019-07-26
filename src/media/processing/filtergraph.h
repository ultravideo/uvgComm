#pragma once

#include <QWidget>
#include <QAudioFormat>

#include <vector>
#include <memory>

class VideoInterface;
class StatisticsInterface;
class AudioOutput;
class Filter;

typedef std::vector<std::shared_ptr<Filter>> GraphSegment;

class FilterGraph
{
public:
  FilterGraph();

  void init(VideoInterface* selfView, StatisticsInterface *stats);
  void uninit();

  // These functions are used to manipulate filter graphs regarding a peer
  void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource);
  void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink, VideoInterface *view);
  void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource);
  void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink);

  // removes participant and all its associated filter from filter graph.
  void removeParticipant(uint32_t sessionID);
  void removeAllParticipants();

  void mic(bool state);
  void camera(bool state);
  void running(bool state);

  // print the filter graph to a dot file to be drawn as a graph
  void print();

  // Refresh settings of all filters from QSettings.
  void updateSettings();

private:

  // adds fitler to graph and connects it to connectIndex unless this is the first filter in graph.
  // adds format conversion if needed.
  bool addToGraph(std::shared_ptr<Filter> filter,
                  GraphSegment& graph,
                  unsigned int connectIndex = 0);

  // connects the two filters and checks for any problems
  bool connectFilters(std::shared_ptr<Filter> filter, std::shared_ptr<Filter> previous);

  // makes sure the participant exists and adds if necessary
  void checkParticipant(uint32_t sessionID);

  // iniates camera and attaches a self view to it.
  void initSelfView(VideoInterface *selfView);

  // iniates encoder and attaches it
  void initVideoSend();

  // iniates encoder and attaches it
  void initAudioSend();

  struct Peer
  {
    // Arrays of filters which send media, but are not connected to each other.
    std::vector<std::shared_ptr<Filter>> audioSenders; // sends audio
    std::vector<std::shared_ptr<Filter>> videoSenders; // sends video

    // Arrays of filters which receive media.
    // Each graphsegment receives one mediastream.
    std::vector<std::shared_ptr<GraphSegment>> videoReceivers;
    std::vector<std::shared_ptr<GraphSegment>> audioReceivers;

    AudioOutput* output; // plays audio coming from this peer
  };

  // destroy all filters associated with this peer.
  void destroyPeer(Peer* peer);

  void destroyFilters(std::vector<std::shared_ptr<Filter>>& filters);


  // id is also the index of the Peer in this vector
  // TODO: Change this to map
  std::vector<Peer*> peers_;

  GraphSegment videoProcessing_;
  GraphSegment audioProcessing_;

  VideoInterface *selfView_;

  StatisticsInterface* stats_;

  uint32_t conversionIndex_;

  // audio configs
  QAudioFormat format_;

  QString videoFormat_;

  bool quitting_;
};
