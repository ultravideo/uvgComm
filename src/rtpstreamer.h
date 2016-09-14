#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>


class FramedSourceFilter;
class RTPSinkFilter;

typedef int16_t PeerID;
/*
struct ConnectionInfo
{
  ip_addr addr;
  uint16_t rtpPort;
  uint16_t rtcpPort;
};
*/

enum ConnectionType {AUDIO, VIDEO};

class RTPStreamer : public QThread
{
  Q_OBJECT

public:
  RTPStreamer(StatisticsInterface* stats);

  //void setDestination(in_addr address, uint16_t port);

  void setPorts(uint16_t audioPort, uint16_t videoPort)
  {
    audioPort_ = audioPort;
    videoPort_ = videoPort;
  }

  void run();

  void stop();


  // use these to ask for filters that are connected to filter graph
  FramedSourceFilter* getSourceFilter(PeerID peer,  ConnectionType type)
  {
    peer_.lock();
    // TODO: Check availability
 //   Q_ASSERT(videoSenders_.find(peer) != videoSenders_.end());
    peer_.unlock();
    if(type == AUDIO)
      return audioSenders_[peer]->framedSource;
    return videoSenders_[peer]->framedSource;
  }

  RTPSinkFilter* getSinkFilter(PeerID peer, ConnectionType type)
  {
    peer_.lock();
//    Q_ASSERT(receivers_.find(peer) != receivers_.end());
    peer_.unlock();
    if(type == AUDIO)
      return audioReceivers_[peer]->sink;
    return videoReceivers_[peer]->sink;
  }

  PeerID addPeer(in_addr peerAddress, uint16_t framerate,
                 bool video, bool audio);

  void removePeer(PeerID id);

private:

  void initLiveMedia();
  void addH265VideoSend(PeerID peer, in_addr peerAddress, uint16_t framerate);
  void addH265VideoReceive(PeerID peer, in_addr peerAddress);
  void uninit();

  struct Connection
  {
    struct in_addr peerAddress;

    Port* rtpPort;
    Port* rtcpPort;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;

    RTCPInstance* rtcp;
  };

  struct Sender
  {
    struct in_addr peerAddress;

    Port* rtpPort;
    Port* rtcpPort;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;

    RTCPInstance* rtcp;

    RTPSink* sink;
    FramedSourceFilter* framedSource; // receives stuff from filter graph
  };

  struct Receiver
  {
    struct in_addr peerAddress;

    Port* rtpPort;
    Port* rtcpPort;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;

    RTCPInstance* rtcp;

    RTPSource* framedSource;
    RTPSinkFilter* sink; // sends stuff to filter graph
  };

  void createConnection(Connection& connection);
  void addSender(bool audio, bool video);
  void addReceiver(bool audio, bool video);
  void destroySenders(std::map<PeerID, Sender*> &senders);
  void destroyReceivers(std::map<PeerID, Receiver*> &receivers);

  std::map<PeerID, Sender*> audioSenders_;
  std::map<PeerID, Sender*> videoSenders_;

  std::map<PeerID, Receiver*> audioReceivers_;
  std::map<PeerID, Receiver*> videoReceivers_;

  PeerID nextID_;
  //bool iniated_;
  QMutex iniated_;
  QMutex peer_;

  uint8_t ttl_;
  struct in_addr sessionAddress_;
  uint16_t audioPort_;
  uint16_t videoPort_;

  char stopRTP_;
  UsageEnvironment* env_;

  StatisticsInterface* stats_;

};

#endif // RTPSTREAMER_H
