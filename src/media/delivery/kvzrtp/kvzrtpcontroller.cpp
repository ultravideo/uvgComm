#include "media/delivery/irtpstreamer.h"
#include "media/delivery/kvzrtp/kvzrtpcontroller.h"
#include "kvzrtpcontroller.h"
#include "kvzrtpsender.h"
#include "kvzrtpreceiver.h"
#include "common.h"

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <kvzrtp/formats/opus.hh>
#include <kvzrtp/lib.hh>

#include <iostream>

KvzRTPController::KvzRTPController():
  rtp_ctx_(new kvz_rtp::context)
{}

KvzRTPController::~KvzRTPController()
{
}

void KvzRTPController::init(StatisticsInterface *stats)
{
  stats_ = stats;
}


void KvzRTPController::uninit()
{
  removeAllPeers();
}


void KvzRTPController::run()
{}


void KvzRTPController::stop()
{}


bool KvzRTPController::addPeer(uint32_t sessionID, QString peerAddress)
{
  Q_ASSERT(sessionID != 0);

  // not being destroyed
  if (destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    printNormal(this, "Adding new peer");

    ipv6to4(peerAddress);

    std::shared_ptr<Peer> peer = std::shared_ptr<Peer> (new Peer{nullptr,{}});
    peers_[sessionID] = peer;
    peers_[sessionID]->session = rtp_ctx_->create_session(peerAddress.toStdString());

    iniated_.unlock();
    destroyed_.unlock();

    if (peers_[sessionID]->session == nullptr)
    {
      removePeer(sessionID);
    }
    printNormal(this, "RTP streamer", { "SessionID" }, { QString::number(sessionID) });

    return true;
  }
  printNormal(this, "Trying to add peer while RTP was being destroyed.");

  return false;
}


void KvzRTPController::parseCodecString(QString codec, uint16_t dst_port,
                      rtp_format_t& fmt, DataType& type, QString& mediaName)
{
  // TODO: This function should better facilitate additional type

  std::map<QString, rtp_format_t> xmap = {
      { "pcm",  RTP_FORMAT_GENERIC },
      { "opus", RTP_FORMAT_OPUS },
      { "h265", RTP_FORMAT_HEVC }
  };

  if (xmap.find(codec) == xmap.end())
  {
    printError(this, "Tried to use non-defined codec type in kvzRTP.");
    type = NONE;
    return;
  }

  fmt  = xmap[codec];
  mediaName = QString::number(dst_port);
  type = NONE;

  switch (fmt)
  {
    case RTP_FORMAT_HEVC:
      mediaName += "_HEVC";
      type = HEVCVIDEO;
      break;

    case RTP_FORMAT_OPUS:
      mediaName += "_OPUS";
      type = OPUSAUDIO;
      break;

    case RTP_FORMAT_GENERIC:
      mediaName += "_RAW_AUDIO";
      type = RAWAUDIO;
      break;

    default :
      printError(this, "RTP support not implemented for this format");
      mediaName += "_UNKNOWN";
      break;
  }
}


std::shared_ptr<Filter> KvzRTPController::addSendStream(uint32_t sessionID, QHostAddress ip,
                                                        uint16_t localPort, uint16_t peerPort,
                                                        QString codec, uint8_t rtpNum)
{
  rtp_format_t fmt;
  DataType type = NONE;
  QString mediaName = "";

  parseCodecString(codec, localPort, fmt, type, mediaName);

  if (!initializeStream(sessionID, ip, localPort, peerPort, fmt))
  {
    return nullptr;
  }

  // create filter if it does not exist
  if (peers_[sessionID]->streams[localPort]->sender == nullptr)
  {
    printNormal(this, "Creating sender filter");

    peers_[sessionID]->streams[localPort]->sender =
        std::shared_ptr<KvzRTPSender>(new KvzRTPSender(ip.toString() + "_",
                                                       stats_,
                                                       type,
                                                       mediaName,
                                                       peers_[sessionID]->streams[localPort]->stream));
  }

  return peers_[sessionID]->streams[localPort]->sender;
}


std::shared_ptr<Filter> KvzRTPController::addReceiveStream(uint32_t sessionID, QHostAddress ip,
                                                           uint16_t localPort, uint16_t peerPort,
                                                           QString codec, uint8_t rtpNum)
{
  rtp_format_t fmt;
  DataType type = NONE;
  QString mediaName = "";

  parseCodecString(codec, localPort, fmt, type, mediaName);

  if (!initializeStream(sessionID, ip, localPort, peerPort, fmt))
  {
    return nullptr;
  }

  // create filter if it does not exist
  if (peers_[sessionID]->streams[localPort]->receiver == nullptr)
  {
    printNormal(this, "Creating receiver filter");
    peers_[sessionID]->streams[localPort]->receiver = std::shared_ptr<KvzRTPReceiver>(
        new KvzRTPReceiver(
          ip.toString() + "_",
          stats_,
          type,
          mediaName,
          peers_[sessionID]->streams[localPort]->stream
        )
    );
  }

  return peers_[sessionID]->streams[localPort]->receiver;
}


bool KvzRTPController::initializeStream(uint32_t sessionID, QHostAddress peerAddress,
                                        uint16_t localPort, uint16_t peerPort,
                                        rtp_format_t fmt)
{
  ipv6to4(peerAddress);

  // add peer if it does not exist
  if (peers_.find(sessionID) == peers_.end())
  {
    if (!addPeer(sessionID, peerAddress.toString()))
    {
      return false;
    }
  }

  // create mediastream if it does not exist
  if (peers_[sessionID]->streams.find(localPort) == peers_[sessionID]->streams.end())
  {
    return addMediaStream(sessionID, localPort, peerPort, fmt);
  }

  return true;
}


bool KvzRTPController::addMediaStream(uint32_t sessionID, uint16_t localPort, uint16_t peerPort,
                                      rtp_format_t fmt)
{
  if (peers_.find(sessionID) == peers_.end())
  {
    return false;
  }

  printNormal(this, "Creating mediastream");

  kvz_rtp::media_stream *stream = peers_[sessionID]->session->create_stream(localPort, peerPort, fmt, 0);

  if (stream == nullptr)
  {
    return false;
  }

  // check if there already exists a media session and overwrite
  if (peers_[sessionID]->streams.find(localPort) != peers_[sessionID]->streams.end() &&
      peers_[sessionID]->streams[localPort] != nullptr)
  {
    printProgramWarning(this, "Existing mediastream detected. Overwriting."
                              " Will cause a crash if previous filters are attached to filtergraph.");

    removeMediaStream(sessionID, localPort);
  }

  // actually create the mediastream
  peers_[sessionID]->streams[localPort] = new MediaStream;
  peers_[sessionID]->streams[localPort]->stream = stream;

  return true;
}


void KvzRTPController::removeMediaStream(uint32_t sessionID, uint16_t localPort)
{
  printNormal(this, "Creating remove mediastream");

  // delete existing streams
  for (auto & stream : peers_[sessionID]->streams)
  {
    if (stream.second != nullptr)
    {
      delete stream.second;
    }
  }

  delete peers_[sessionID]->streams[localPort];
  peers_[sessionID]->streams[localPort] = nullptr;
  peers_[sessionID]->streams.erase(localPort);
}


void KvzRTPController::removePeer(uint32_t sessionID)
{
  if (peers_.find(sessionID) != peers_.end())
  {
    std::vector<uint16_t> streams;

    // take all keys so we wont get iterator errors
    for (auto& stream : peers_[sessionID]->streams)
    {
      streams.push_back(stream.first);
    }

    // delete all streams
    for (auto& stream : streams)
    {
      removeMediaStream(sessionID, stream);
    }

    // delete session
    if (peers_[sessionID]->session)
    {
      delete peers_[sessionID]->session;
      peers_[sessionID]->session = nullptr;
    }

    peers_.erase(sessionID);
  }
}


void KvzRTPController::removeAllPeers()
{
  std::vector<uint32_t> ids;

  // take all ids so we wont get iterator errors
  for (auto& peer : peers_)
  {
    ids.push_back(peer.first);
  }

  // remove all peers by sessionID
  for (unsigned int i = 0; i < ids.size(); ++i)
  {
    removePeer(i);
  }
}

void KvzRTPController::ipv6to4(QHostAddress& address)
{
  // check if the IP addresses start with ::ffff: and remove it, kvzrtp only accepts IPv4 addresses
  if (address.toString().left(7) == "::ffff:")
  {
    address = QHostAddress(address.toString().mid(7));
  }
}

void KvzRTPController::ipv6to4(QString &address)
{
  // check if the IP addresses start with ::ffff: and remove it, kvzrtp only accepts IPv4 addresses
  if (address.left(7) == "::ffff:")
  {
    address = address.mid(7);
  }
}
