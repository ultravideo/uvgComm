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

class KvzRTPController : public IRTPStreamer
{
  Q_OBJECT

public:
  KvzRTPController();
  ~KvzRTPController();

   void init(StatisticsInterface *stats);
   void uninit();
   void run();
   void stop();

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
   bool addPeer(uint32_t sessionID, QString peerAddress);

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
  std::shared_ptr<Filter> addSendStream(uint32_t peer, QHostAddress ip,
                                        uint16_t localPort, uint16_t peerPort,
                                        QString codec, uint8_t rtpNum);

  std::shared_ptr<Filter> addReceiveStream(uint32_t peer, QHostAddress ip,
                                           uint16_t localPort, uint16_t peerPort,
                                           QString codec, uint8_t rtpNum);

   void removeSendVideo(uint32_t sessionID);
   void removeSendAudio(uint32_t sessionID);
   void removeReceiveVideo(uint32_t sessionID);
   void removeReceiveAudio(uint32_t sessionID);

  // removes everything related to this peer
   void removePeer(uint32_t sessionID);

   void removeAllPeers();

private:
  void addAudioMediaStream(uint32_t peer, QHostAddress ip, uint16_t src_port,
                           uint16_t dst_port, DataType realType, QString mediaName, rtp_format_t fmt);

  void addVideoMediaStream(uint32_t peer, QHostAddress ip, uint16_t src_port,
                           uint16_t dst_port, DataType realType, QString mediaName, rtp_format_t fmt);

  void parseCodecString(QString codec, uint16_t dst_port,
                        rtp_format_t& fmt, DataType& type, QString& mediaName);

  kvz_rtp::context *rtp_ctx_;


  struct MediaStream
  {
    kvz_rtp::media_stream *stream;

    std::shared_ptr<KvzRTPSender> sender;
    std::shared_ptr<KvzRTPReceiver> receiver;
  };

  struct Peer
  {
    kvz_rtp::session *session;

    MediaStream *video;
    MediaStream *audio;
  };

  rtp_format_t typeFromString(QString type);

  // returns whether peer corresponding to sessionID has been created. Debug
  bool checkSessionID(uint32_t sessionID);

  // private variables
  std::map<uint32_t, std::shared_ptr<Peer>> peers_;

  QMutex iniated_; // locks for duration of creation
  QMutex destroyed_; // locks for duration of destruction

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  StatisticsInterface *stats_;
};
