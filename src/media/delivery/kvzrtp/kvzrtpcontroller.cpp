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

KvzRTPController::KvzRTPController()
{
  isIniated_ = false;
  rtp_ctx_ = new kvz_rtp::context;
}

KvzRTPController::~KvzRTPController()
{
}

void KvzRTPController::init(StatisticsInterface *stats)
{
  stats_ = stats;
  isIniated_ = true;
}

void KvzRTPController::uninit()
{
  removeAllPeers();
  isIniated_ = false;
}

void KvzRTPController::run()
{
  if (!isIniated_)
  {
    init(stats_);
  }
}

void KvzRTPController::stop()
{
  isRunning_ = false;
}

bool KvzRTPController::checkSessionID(uint32_t sessionID)
{
  return peers_.find(sessionID) != peers_.end();
}

bool KvzRTPController::addPeer(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  // not being destroyed
  if (destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    printDebug(DEBUG_NORMAL, this, "Adding new peer");

    std::shared_ptr<Peer> peer = std::shared_ptr<Peer> (new Peer{nullptr,nullptr,nullptr});
    peers_[sessionID] = peer;

    iniated_.unlock();
    destroyed_.unlock();

    printDebug(DEBUG_NORMAL, this, "RTP streamer", { "SessionID" },
    { QString::number(sessionID) });

    return true;
  }
  printDebug(DEBUG_WARNING, this,
             "Trying to add peer while RTP was being destroyed.");

  return false;
}


rtp_format_t KvzRTPController::typeFromString(QString type)
{
  std::map<QString, rtp_format_t> xmap = {
      { "pcm",  RTP_FORMAT_GENERIC },
      { "opus", RTP_FORMAT_OPUS },
      { "h265", RTP_FORMAT_HEVC }
  };

  if (xmap.find(type) == xmap.end())
  {
    printDebug(DEBUG_ERROR, this, "Tried to use non-defined codec type in kvzRTP.");
    return RTP_FORMAT_GENERIC;
  }

  return xmap[type];
}


void KvzRTPController::parseCodecString(QString codec, uint16_t dst_port,
                      rtp_format_t& fmt, DataType& type, QString& mediaName)
{
  fmt  = typeFromString(codec);
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


std::shared_ptr<Filter> KvzRTPController::addSendStream(uint32_t peer, QHostAddress ip,
                                              uint16_t localPort, uint16_t peerPort,
                                              QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));
  // check if the IP addresses start with ::ffff: and remove it, kvzrtp only accepts IPv4 addresses
  if (ip.toString().left(7) == "::ffff:")
  {
    ip = QHostAddress(ip.toString().mid(7));
  }
  rtp_format_t fmt;
  DataType type = NONE;
  QString mediaName = "";

  parseCodecString(codec, localPort, fmt, type, mediaName);

  if (fmt == RTP_FORMAT_HEVC)
  {
    if (peers_[peer]->video == nullptr)
    {
      addVideoMediaStream(peer, ip, localPort, peerPort, type, mediaName, fmt);
    }

    return peers_[peer]->video->sender;
  }
  else if (fmt == RTP_FORMAT_OPUS)
  {
    if (peers_[peer]->audio == nullptr)
    {
      addAudioMediaStream(peer, ip, peerPort, localPort, type, mediaName, fmt);
    }

    return peers_[peer]->audio->sender;
  }

  return nullptr;
}


std::shared_ptr<Filter> KvzRTPController::addReceiveStream(uint32_t peer, QHostAddress ip, uint16_t localPort, uint16_t peerPort,
                                                 QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));
  // check if the IP addresses start with ::ffff: and remove it, kvzrtp only accepts IPv4 addresses
  if (ip.toString().left(7) == "::ffff:")
  {
    ip = QHostAddress(ip.toString().mid(7));
  }
  rtp_format_t fmt;
  DataType type = NONE;
  QString mediaName = "";

  parseCodecString(codec, localPort, fmt, type, mediaName);

  if (fmt == RTP_FORMAT_HEVC)
  {
    if (peers_[peer]->video == nullptr)
    {
      addVideoMediaStream(peer, ip, localPort, peerPort, type, mediaName, fmt);
    }
    return peers_[peer]->video->receiver;
  }
  else if (fmt == RTP_FORMAT_OPUS)
  {
    if (peers_[peer]->audio == nullptr)
    {
      addAudioMediaStream(peer, ip, localPort, peerPort, type, mediaName, fmt);

    }
    return peers_[peer]->audio->receiver;
  }

  return nullptr;
}


void KvzRTPController::addAudioMediaStream(uint32_t peer, QHostAddress ip, uint16_t src_port,
                                 uint16_t dst_port, DataType realType, QString mediaName, rtp_format_t fmt)
{
  peers_[peer]->audio    = new MediaStream;

  peers_[peer]->audio->stream =
    new kvz_rtp::media_stream(ip.toString().toStdString(), src_port, dst_port, fmt, 0);

  (void)peers_[peer]->audio->stream->init();

  peers_[peer]->audio->sender = std::shared_ptr<KvzRTPSender>(
    new KvzRTPSender(
      ip.toString() + "_",
      stats_,
      realType,
      mediaName,
      peers_[peer]->audio->stream
    )
  );

  peers_[peer]->audio->receiver = std::shared_ptr<KvzRTPReceiver>(
      new KvzRTPReceiver(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        peers_[peer]->audio->stream
      )
  );
}


void KvzRTPController::addVideoMediaStream(uint32_t peer, QHostAddress ip, uint16_t src_port,
                                 uint16_t dst_port, DataType realType, QString mediaName, rtp_format_t fmt)
{
  peers_[peer]->video    = new MediaStream;

  peers_[peer]->video->stream =
    new kvz_rtp::media_stream(ip.toString().toStdString(), src_port, dst_port, fmt, 0);

  (void)peers_[peer]->video->stream->init();

  peers_[peer]->video->sender = std::shared_ptr<KvzRTPSender>(
    new KvzRTPSender(
      ip.toString() + "_",
      stats_,
      realType,
      mediaName,
      peers_[peer]->video->stream
    )
  );

  peers_[peer]->video->receiver = std::shared_ptr<KvzRTPReceiver>(
      new KvzRTPReceiver(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        peers_[peer]->video->stream
      )
  );
}


void KvzRTPController::removePeer(uint32_t sessionID)
{
  Q_ASSERT(sessionID && checkSessionID(sessionID));

  if (checkSessionID(sessionID))
  {
    // delete session
    if (peers_[sessionID]->session)
    {
      delete peers_[sessionID]->session;
      peers_[sessionID]->session = nullptr;
    }

    // delete audio
    if (peers_[sessionID]->audio)
    {
      if (peers_[sessionID]->audio->stream)
      {
        delete peers_[sessionID]->audio->stream;
        peers_[sessionID]->audio->stream = nullptr;
      }

      delete peers_[sessionID]->audio;
      peers_[sessionID]->audio = nullptr;
    }

    // delete video
    if (peers_[sessionID]->video)
    {
      if (peers_[sessionID]->video->stream)
      {
        delete peers_[sessionID]->video->stream;
        peers_[sessionID]->video->stream = nullptr;
      }

      delete peers_[sessionID]->video;
      peers_[sessionID]->video = nullptr;
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
