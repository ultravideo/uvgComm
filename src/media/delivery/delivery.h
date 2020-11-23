#pragma once
#include "media/processing/filter.h"

#include <QMutex>
#include <QHostAddress>
#include <QObject>
#include <QFuture>

#include <uvgrtp/lib.hh>

#include <vector>

class StatisticsInterface;
class UvgRTPSender;
class UvgRTPReceiver;
class Filter;

class Delivery : public QObject
{
  Q_OBJECT

public:
  Delivery();
  ~Delivery();

   void init(StatisticsInterface *stats);
   void uninit();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
   bool addPeer(uint32_t sessionID, QString peerAddress, QString localAddress);

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

signals:
  void handleZRTPFailure(uint32_t sessionID);

private:

  struct MediaStream
  {
    QFuture<uvg_rtp::media_stream *> stream;

   std::shared_ptr<UvgRTPSender> sender;
   std::shared_ptr<UvgRTPReceiver> receiver;
  };

  struct Peer
  {
   uvg_rtp::session *session;

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

  uvg_rtp::context *rtp_ctx_;

  QMutex iniated_; // locks for duration of creation
  QMutex destroyed_; // locks for duration of destruction

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  StatisticsInterface *stats_;
};
