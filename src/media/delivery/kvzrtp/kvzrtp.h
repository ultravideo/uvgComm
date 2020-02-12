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

class KvzRTP : public IRTPStreamer
{
  Q_OBJECT

public:
  KvzRTP();
  ~KvzRTP();

   void init(StatisticsInterface *stats);
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
                                           uint16_t port, QString codec, uint8_t rtpNum);

  /* Source is "first", sink is "second" */
  std::pair<
    std::shared_ptr<Filter>,
    std::shared_ptr<Filter>
  >
  addMediaStream(uint32_t peer, QHostAddress ip, uint16_t src_port,
                 uint16_t dst_port, QString codec);

   void removeSendVideo(uint32_t sessionID);
   void removeSendAudio(uint32_t sessionID);
   void removeReceiveVideo(uint32_t sessionID);
   void removeReceiveAudio(uint32_t sessionID);

  // removes everything related to this peer
   void removePeer(uint32_t sessionID);

   void removeAllPeers();

private:
  kvz_rtp::context *rtp_ctx_;

  struct MediaStream
  {
    kvz_rtp::media_stream *stream;

    std::shared_ptr<KvzRTPSender> source;
    std::shared_ptr<KvzRTPReceiver> sink;
  };

  struct Peer
  {
    QHostAddress video_ip;
    QHostAddress audio_ip;

    kvz_rtp::session *session;

    MediaStream *video;
    MediaStream *audio;
  };

  rtp_format_t typeFromString(QString type);

  // returns whether peer corresponding to sessionID has been created. Debug
  bool checkSessionID(uint32_t sessionID);

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
