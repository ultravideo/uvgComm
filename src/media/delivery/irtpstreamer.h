#pragma once
#include "media/processing/filter.h"

#include <QHostAddress>

class IRTPStreamer : public QThread
{
public:
  virtual ~IRTPStreamer(){}

  virtual void init(StatisticsInterface *stats) = 0;
  virtual void uninit() = 0;
  virtual void stop() = 0;

  // init a session with sessionID to use with add/remove functions
  // returns whether operation was successful
  virtual bool addPeer(uint32_t sessionID, QString peerAddress) = 0;

  // Returns filter to be attached to filter graph. ownership is not transferred.
  // removing the peer or stopping the streamer destroys these filters.
  virtual std::shared_ptr<Filter> addSendStream(uint32_t peer, QHostAddress ip,
                                                uint16_t localPort, uint16_t peerPort,
                                                QString codec, uint8_t rtpNum) = 0;

  virtual std::shared_ptr<Filter> addReceiveStream(uint32_t peer, QHostAddress ip,
                                                   uint16_t localPort, uint16_t peerPort,
                                                   QString codec, uint8_t rtpNum) = 0;

  // removes everything related to this peer
  virtual void removePeer(uint32_t sessionID) = 0;

  virtual void removeAllPeers() = 0;
};
