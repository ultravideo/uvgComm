#pragma once
#include "media/processing/filter.h"
#include "media/delivery/irtpstreamer.h"

#include <QThread>
#include <QMutex>
#include <QHostAddress>
#include <vector>

#include <kvzrtp/lib.hh>

class StatisticsInterface;
class KvzRTPSender;
class KvzRTPReceiver;
class Filter;

class Delivery : public IRTPStreamer
{
  Q_OBJECT

public:
  Delivery();
  ~Delivery();

   void init(StatisticsInterface *stats);
   void uninit();
   void run();
   void stop();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
   bool addPeer(uint32_t sessionID, QString peerAddress);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
  std::shared_ptr<Filter> addSendStream(uint32_t sessionID, QHostAddress remoteAddress,
                                        uint16_t localPort, uint16_t peerPort,
                                        QString codec, uint8_t rtpNum);

  std::shared_ptr<Filter> addReceiveStream(uint32_t sessionID, QHostAddress localAddress,
                                           uint16_t localPort, uint16_t peerPort,
                                           QString codec, uint8_t rtpNum);

  // TODO
  //void removeSendStream(uint32_t sessionID, uint16_t localPort);
  //void removeReceiveStream(uint32_t sessionID, uint16_t localPort);

  // removes everything related to this peer
   void removePeer(uint32_t sessionID);

   void removeAllPeers();

private:

  struct MediaStream
  {
   kvz_rtp::media_stream *stream;

   std::shared_ptr<KvzRTPSender> sender;
   std::shared_ptr<KvzRTPReceiver> receiver;
  };

  struct Peer
  {
   kvz_rtp::session *session;

   // uses local port as key
   std::map<uint16_t, MediaStream*> streams;
  };

  bool initializeStream(uint32_t sessionID, uint16_t localPort, uint16_t peerPort,
                        rtp_format_t fmt);

  bool addMediaStream(uint32_t sessionID, uint16_t localPort, uint16_t peerPort,
                      rtp_format_t fmt);
  void removeMediaStream(uint32_t sessionID, uint16_t localPort);

  void parseCodecString(QString codec, uint16_t dst_port,
                        rtp_format_t& fmt, DataType& type, QString& mediaName);

  void ipv6to4(QHostAddress &address);
  void ipv6to4(QString &address);

  // private variables
  std::map<uint32_t, std::shared_ptr<Peer>> peers_;

  kvz_rtp::context *rtp_ctx_;

  QMutex iniated_; // locks for duration of creation
  QMutex destroyed_; // locks for duration of destruction

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  StatisticsInterface *stats_;
};
