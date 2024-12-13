#pragma once
#include "media/delivery/udprelay.h"
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

   void init(StatisticsInterface *stats, std::shared_ptr<ResourceAllocator> hwResources);
   void uninit();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
   bool addSession(uint32_t sessionID,
                   QString peerAddressType, QString peerAddress,
                   QString localAddressType, QString localAddress);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
   std::shared_ptr<Filter> addRTPSendStream(uint32_t sessionID,
                                            QString localAddress,
                                            QString remoteAddress,
                                            uint16_t localPort,
                                            uint16_t peerPort,
                                            QString codec,
                                            uint8_t rtpNum,
                                            uint32_t localSSRC = 0,
                                            uint32_t remoteSSRC = 0);

   std::shared_ptr<Filter> addUDPSendStream(uint32_t sessionID,
                                            QString localAddress,
                                            QString remoteAddress,
                                            uint16_t localPort,
                                            uint16_t peerPort,
                                            uint32_t remoteSSRC);

   std::shared_ptr<Filter> addRTPReceiveStream(uint32_t sessionID,
                                               QString localAddress,
                                               QString remoteAddress,
                                               uint16_t localPort,
                                               uint16_t peerPort,
                                               QString codec,
                                               uint8_t rtpNum,
                                               uint32_t localSSRC = 0,
                                               uint32_t remoteSSRC = 0);

   std::shared_ptr<Filter> addUDPReceiveStream(uint32_t sessionID,
                                               QString localAddress, uint16_t localPort,
                                               uint32_t remoteSSRC);

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

  struct SendStream
  {
    uvgrtp::media_stream *ms;
    bool runZRTP;

    std::shared_ptr<UvgRTPSender> sender;
  };


  struct RecvStream
  {
    uvgrtp::media_stream *ms;
    bool runZRTP;

    // key is remote SSRC
    std::shared_ptr<UvgRTPReceiver> receiver;
  };


  struct DeliverySession
  {
    uvgrtp::session *session;

    QString localAddress;
    QString peerAddress;

    // key is local SSRC
    std::map<uint32_t, SendStream*> outgoingStreams;

    // key is remote SSRC
    std::map<uint32_t, RecvStream*> incomingStreams;
    bool dhSelected;
  };

  struct Peer
  {
    std::vector<DeliverySession> sessions;
  };

  bool initializeStream(uint32_t sessionID,
                        DeliverySession &session,
                        uint16_t localPort,
                        uint16_t peerPort,
                        rtp_format_t fmt,
                        uint32_t localSSRC,
                        uint32_t remoteSSRC,
                        bool recv);

  bool addMediaStream(uint32_t sessionID,
                      DeliverySession &session,
                      uint16_t localPort,
                      uint16_t peerPort,
                      rtp_format_t fmt,
                      bool dhSelected,
                      uint32_t localSSRC,
                      uint32_t remoteSSRC,
                      bool recv);

  void removeSendStream(uint32_t sessionID, DeliverySession& session,
                         uint32_t localSSRC);

  void removeRecvStream(uint32_t sessionID, DeliverySession& session,
                         uint32_t remoteSSRC);

  void parseCodecString(QString codec, rtp_format_t& fmt,
                        DataType& type, QString& mediaName);

  bool findSession(uint32_t sessionID, uint32_t& outIndex,
                   QString localAddress, QString remoteAddress);

  std::shared_ptr<UDPRelay> getUDPRelay(QString localAddress, uint16_t localPort);

  // key is sessionID
  std::map<uint32_t, std::shared_ptr<Peer>> peers_;

  uvg_rtp::context *rtp_ctx_;

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  StatisticsInterface *stats_;

  std::shared_ptr<ResourceAllocator> hwResources_;

  // the key is address:port as a string
  std::map<QString, std::shared_ptr<UDPRelay>> relays_;

  // key is
  std::map<uint32_t, std::shared_ptr<Filter>> udpSenders_;
  std::map<uint32_t, std::shared_ptr<Filter>> udpReceivers_;
};
