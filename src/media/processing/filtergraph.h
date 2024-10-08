#pragma once

#include "mediaid.h"
#include <QWidget>
#include <QtMultimedia/QAudioFormat>
#include <QObject>

#include <vector>
#include <memory>

class VideoInterface;

class Filter;
class ResourceAllocator;
class StatisticsInterface;


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
  virtual void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                           const MediaID &id) = 0;

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                        VideoInterface *view,
                        const MediaID &id) = 0;

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                   const MediaID &id) = 0;

  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                        const MediaID &id) = 0;

  // removes participant and all its associated filter from filter graph.
  void removeParticipant(uint32_t sessionID);

  void running(bool state);

public slots:

  virtual void updateVideoSettings();
  virtual void updateAudioSettings();
  virtual void updateAutomaticSettings() = 0;

protected:

  struct Peer
  {
    // keep track of existing connections, so we don't duplicate them
    std::vector<MediaID> sendingStreams;
    std::vector<MediaID> receivingStreams;

    // Arrays of filters which send media, but are not connected to each other.
    std::vector<std::shared_ptr<Filter>> audioSenders; // sends audio
    std::vector<std::shared_ptr<Filter>> videoSenders; // sends video

    // Arrays of filters which receive media.
    // Each graphsegment receives one mediastream.
    std::vector<std::shared_ptr<GraphSegment>> videoReceivers;
    std::vector<std::shared_ptr<GraphSegment>> audioReceivers;
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

  bool existingConnection(std::vector<MediaID>& connections, MediaID id);

  void removeAllParticipants();

  // --------------- General stuff ----------------

  bool quitting_;

  std::shared_ptr<ResourceAllocator> hwResources_;
  StatisticsInterface* stats_;

  // key is sessionID
  std::map<uint32_t, Peer*> peers_;
};
