#pragma once
#include "media/processing/filter.h"
#include "media/delivery/irtpstreamer.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include <QThread>
#include <QMutex>
#include <QHostAddress>

#include <vector>

class FramedSourceFilter;
class RTPSinkFilter;
class Filter;
class StatisticsInterface;

class Live555RTP : public IRTPStreamer
{
  Q_OBJECT

public:
  Live555RTP();
  ~Live555RTP();
  void init(StatisticsInterface* stats);
  void uninit();
  void run();
  void stop();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
  bool addPeer(uint32_t sessionID);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
  std::shared_ptr<Filter> addSendStream(uint32_t peer, QHostAddress ip,
                                        uint16_t dst_port, uint16_t src_port,
                                        QString codec, uint8_t rtpNum);

  std::shared_ptr<Filter> addReceiveStream(uint32_t peer, QHostAddress ip,
                                           uint16_t port, QString codec,
                                           uint8_t rtpNum);


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
    //H265VideoStreamFramer* framerSource;
    H265VideoStreamDiscreteFramer* framerSource;
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
    std::vector<Live555RTP::Sender *> senders;
    std::vector<Live555RTP::Receiver *> receivers;
  };

  void createConnection(Connection& connection,
                        struct in_addr ip, uint16_t portNum,
                        bool reservePorts);

  void destroyConnection(Connection& connection);

  // returns whether peer corresponding to sessionID has been created. Debug
  bool checkSessionID(uint32_t sessionID);

  Sender* addSender(in_addr ip, uint16_t port, DataType type, uint8_t rtpNum);
  Receiver* addReceiver(in_addr peerAddress, uint16_t port, DataType type, uint8_t rtpNum);

  void destroySender(Sender* sender);
  void destroyReceiver(Receiver* recv);

  DataType typeFromString(QString type);

  QList<Peer*> peers_;

  bool isIniated_;
  bool isRunning_;

  QMutex iniated_; // locks for duration of creation
  QMutex destroyed_; // locks for duration of destruction

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  char stopRTP_; // char for stopping live555 taskscheduler
  UsageEnvironment* env_;
  TaskScheduler* scheduler_; // pointer needed for proper destruction

  StatisticsInterface* stats_;

  static const unsigned int maxCNAMElen_ = 100;
  unsigned char CNAME_[maxCNAMElen_ + 1];

  QMutex* triggerMutex_;
};
