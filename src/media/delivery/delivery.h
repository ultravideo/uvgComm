#pragma once
#include "media/processing/filter.h"
#include "mediaid.h"

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

   void init(StatisticsInterface *stats, std::shared_ptr<ResourceAllocator> hwResources);
   void uninit();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
   bool addSession(uint32_t sessionID,
                   QString peerAddressType, QString peerAddress,
                   QString localAddressType, QString localAddress);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
  std::shared_ptr<Filter> addSendStream(uint32_t sessionID,
                                        QString localAddress, QString remoteAddress,
                                        uint16_t localPort, uint16_t peerPort,
                                        QString codec, uint8_t rtpNum, MediaID id,
                                         uint32_t localSSRC = 0, uint32_t remoteSSRC = 0);

  std::shared_ptr<Filter> addReceiveStream(uint32_t sessionID,
                                           QString localAddress, QString remoteAddress,
                                           uint16_t localPort, uint16_t peerPort,
                                           QString codec, uint8_t rtpNum, MediaID id,
                                           uint32_t localSSRC = 0, uint32_t remoteSSRC = 0);

  // TODO: Add a way to remove individual streams
  //void removeSendStream(uint32_t sessionID, uint16_t localPort);
  //void removeReceiveStream(uint32_t sessionID, uint16_t localPort);

  // removes everything related to this peer
   void removePeer(uint32_t sessionID);

   void removeAllPeers();

signals:
  void handleZRTPFailure(uint32_t sessionID);
  void handleNoEncryption();

private:

  struct MediaStream
  {
    std::shared_ptr<UvgRTPStream> stream;

    std::shared_ptr<UvgRTPSender> sender;
    std::shared_ptr<UvgRTPReceiver> receiver;
  };

  struct DeliverySession
  {
    uvg_rtp::session *session;

    QString localAddress;
    QString peerAddress;

    std::map<MediaID, MediaStream*> streams;
    bool dhSelected;
  };

  struct Peer
  {
    std::vector<DeliverySession> sessions;
  };

  bool initializeStream(uint32_t sessionID,
                        DeliverySession& session,
                        uint16_t localPort, uint16_t peerPort, MediaID id,
                        rtp_format_t fmt, uint32_t localSSRC, uint32_t remoteSSRC);

  bool addMediaStream(uint32_t sessionID,
                      DeliverySession& session,
                      uint16_t localPort, uint16_t peerPort,
                      rtp_format_t fmt, bool dhSelected, MediaID &id, uint32_t localSSRC, uint32_t remoteSSRC);
  void removeMediaStream(uint32_t sessionID, DeliverySession& session, MediaID &id);

  void parseCodecString(QString codec, rtp_format_t& fmt,
                        DataType& type, QString& mediaName);

  bool findSession(uint32_t sessionID, uint32_t& outIndex,
                   QString localAddress, QString remoteAddress);

  // key is sessionID
  std::map<uint32_t, std::shared_ptr<Peer>> peers_;

  uvg_rtp::context *rtp_ctx_;

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  StatisticsInterface *stats_;

  std::shared_ptr<ResourceAllocator> hwResources_;


};
