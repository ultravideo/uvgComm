#pragma once

#include <QWidget>
#include <QtMultimedia/QAudioFormat>
#include <QObject>

#include <vector>
#include <memory>

class VideoInterface;

class Filter;
class ResourceAllocator;
class StatisticsInterface;
class LibYUVConverter;


typedef std::vector<std::shared_ptr<Filter>> GraphSegment;

class FilterGraph : public QObject
{
  Q_OBJECT
public:
  FilterGraph();

  void init(StatisticsInterface *stats,
            std::shared_ptr<ResourceAllocator> hwResources);

  virtual void uninit() = 0;

  // These functions are used to manipulate filter graphs regarding a peer
  virtual void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> sender,
                           uint32_t localSSRC) = 0;

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                        VideoInterface *view,
                        uint32_t remoteSSRC) = 0;

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> sender,
                   uint32_t localSSRC) = 0;

  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                        uint32_t remoteSSRC) = 0;

  // removes participant and all its associated filter from filter graph.
  void removeParticipant(uint32_t sessionID);

  virtual void running(bool state);

public slots:

  virtual void updateVideoSettings();
  virtual void updateAudioSettings();
  virtual void updateAutomaticSettings();

protected:

  struct Peer
  {
    // key is local SSRC
    std::map<uint32_t, std::shared_ptr<Filter>> audioSenders; // sends audio
    std::map<uint32_t, std::shared_ptr<Filter>> videoSenders; // sends video

    // key is remote SSRC
    std::map<uint32_t, std::shared_ptr<GraphSegment>> videoReceivers;
    std::map<uint32_t, std::shared_ptr<GraphSegment>> audioReceivers;
  };

  // destroy all filters associated with this peer.
  virtual void destroyPeer(Peer* peer);

  virtual void lastPeerRemoved() = 0;

  // makes sure the participant exists and adds if necessary
  void checkParticipant(uint32_t sessionID);

  // Adds fitler to graph and connects it to connectIndex unless this is
  // the first filter in graph. Adds format conversion if needed.
  bool addToGraph(std::shared_ptr<Filter> filter,
                  GraphSegment& graph,
                  size_t connectIndex = 0);

  // connects the two filters and checks for any problems
  bool connectFilters(std::shared_ptr<Filter> previous, std::shared_ptr<Filter> filter);

  void changeState(std::shared_ptr<Filter> f, bool state);


  void destroyFilters(std::vector<std::shared_ptr<Filter>>& filters);

  void removeAllParticipants();

  // --------------- General stuff ----------------

  bool quitting_;

  std::shared_ptr<ResourceAllocator> hwResources_;
  StatisticsInterface* stats_;

  // key is sessionID
  std::map<uint32_t, Peer*> peers_;
};
