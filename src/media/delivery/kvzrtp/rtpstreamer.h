#pragma once
#include "media/processing/filter.h"
#include "media/delivery/irtpstreamer.h"

#include <QThread>
#include <QMutex>
#include <QHostAddress>
#include <vector>

#include <kvzrtp/reader.hh>
#include <kvzrtp/lib.hh>

class StatisticsInterface;
class FramedSourceFilter;
class RTPSinkFilter;
class Filter;

class RTPStreamer : public QThread, public IRTPStreamer
{
  Q_OBJECT

public:
  RTPStreamer();
  ~RTPStreamer();

   void init(StatisticsInterface *stats);
   void uninit();
   void run();
   void stop();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
   bool addPeer(QHostAddress address, uint32_t sessionID);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
   std::shared_ptr<Filter> addSendStream(uint32_t peer, uint16_t port, QString codec, uint8_t rtpNum);
   std::shared_ptr<Filter> addReceiveStream(uint32_t peer, uint16_t port, QString codec, uint8_t rtpNum);

   void removeSendVideo(uint32_t sessionID);
   void removeSendAudio(uint32_t sessionID);
   void removeReceiveVideo(uint32_t sessionID);
   void removeReceiveAudio(uint32_t sessionID);

  // removes everything related to this peer
   void removePeer(uint32_t sessionID);

   void removeAllPeers();

private:
  kvz_rtp::context rtp_ctx_;

  struct Sender
  {
    kvz_rtp::writer *writer;
    std::shared_ptr<FramedSourceFilter> sourcefilter; // receives stuff from filter graph
  };

  struct Receiver
  {
    kvz_rtp::reader *reader;
    std::shared_ptr<RTPSinkFilter> sink; // sends stuff to filter graph
  };

  struct Peer
  {
    struct in_addr ip; // TODO who's ip is this???
    QHostAddress ip_str;

    Sender *audioSender; // audio to this peer
    Sender *videoSender; // video to this peer

    Receiver *audioReceiver; // audio from this peer
    Receiver *videoReceiver; // video from this peer
  };

  void destroySender(Sender *sender);
  void destroyReceiver(Receiver *recv);
  rtp_format_t typeFromString(QString type);

  // returns whether peer corresponding to sessionID has been created. Debug
  bool checkSessionID(uint32_t sessionID);

  Sender     *addSender(QHostAddress ip, uint16_t port, rtp_format_t type, uint8_t rtpNum);
  Receiver *addReceiver(QHostAddress ip, uint16_t port, rtp_format_t type, uint8_t rtpNum);
#if 0
#endif

  // private variables
  QList<Peer *> peers_;

  bool isIniated_;
  bool isRunning_;

  QMutex iniated_; // locks for duration of creation
  QMutex destroyed_; // locks for duration of destruction

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  StatisticsInterface *stats_;
};
