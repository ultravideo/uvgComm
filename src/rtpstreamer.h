#pragma once
#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include <QThread>

#include <vector>

class FramedSourceFilter;
class RTPSinkFilter;
class Filter;
class StatisticsInterface;

class RTPStreamer : public QThread
{
  Q_OBJECT

public:
  RTPStreamer();

  void init(StatisticsInterface* stats);
  void uninit();
  void run();
  void stop();

  // init a session with sessionID to use with add/remove functions
  // returns whther opeartion was successful
  bool addPeer(in_addr ip, uint32_t sessionID);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing peer and stopping destroy these filters.
  std::shared_ptr<Filter> addSendVideo(uint32_t peer, uint16_t port);
  std::shared_ptr<Filter> addSendAudio(uint32_t peer, uint16_t port);
  std::shared_ptr<Filter> addReceiveVideo(uint32_t peer, uint16_t port);
  std::shared_ptr<Filter> addReceiveAudio(uint32_t peer, uint16_t port);

  void removeSendVideo(uint32_t sessionID);
  void removeSendAudio(uint32_t sessionID);
  void removeReceiveVideo(uint32_t sessionID);
  void removeReceiveAudio(uint32_t sessionID);

  // removes everything related to this peer
  void removePeer(uint32_t sessionID);

  void removeAllPeers();

private:

  struct Connection
  {
    Port* rtpPort;
    Port* rtcpPort;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;
  };

  struct Sender
  {
    Connection connection;

    RTCPInstance* rtcp;

    RTPSink* sink;
    std::shared_ptr<FramedSourceFilter> sourcefilter; // receives stuff from filter graph
  };

  struct Receiver
  {
    Connection connection;

    RTCPInstance* rtcp;

    RTPSource* framedSource;
    std::shared_ptr<RTPSinkFilter> sink; // sends stuff to filter graph
  };

  struct Peer
  {
    struct in_addr ip;
    Sender* audioSender; // audio to this peer
    Sender* videoSender; // video to this peer

    Receiver* audioReceiver; // audio from this peer
    Receiver* videoReceiver; // video from this peer
  };

  void createConnection(Connection& connection,
                        struct in_addr ip, uint16_t portNum,
                        bool reservePorts);

  void destroyConnection(Connection& connection);

  // returns whether peer corresponding to sessionID has been created. Debug
  bool checkSessionID(uint32_t sessionID);

  Sender* addSender(in_addr ip, uint16_t port, DataType type);
  Receiver* addReceiver(in_addr peerAddress, uint16_t port, DataType type);

  void destroySender(Sender* sender);
  void destroyReceiver(Receiver* recv);

  QList<Peer*> peers_;

  bool isIniated_;
  bool isRunning_;

  QMutex iniated_; // locks for duration of creation
  QMutex destroyed_; // locks for duration of

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  char stopRTP_; // char for stopping live555 taskscheduler
  UsageEnvironment* env_;
  TaskScheduler* scheduler_; // pointer needed for proper destruction

  StatisticsInterface* stats_;

  static const unsigned int maxCNAMElen_ = 100;
  unsigned char CNAME_[maxCNAMElen_ + 1];
};
