#include "media/delivery/irtpstreamer.h"
#include "media/delivery/kvzrtp/kvzrtp.h"
#include "kvzrtp.h"
#include "kvzrtpsender.h"
#include "kvzrtpreceiver.h"
#include "common.h"

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <kvzrtp/formats/opus.hh>
#include <kvzrtp/lib.hh>

#include <iostream>

KvzRTP::KvzRTP()
{
  isIniated_ = false;
  rtp_ctx_ = new kvz_rtp::context;
}

KvzRTP::~KvzRTP()
{
}

void KvzRTP::init(StatisticsInterface *stats)
{
  stats_ = stats;
  isIniated_ = true;
}

void KvzRTP::uninit()
{
  removeAllPeers();
  isIniated_ = false;
}

void KvzRTP::run()
{
  if (!isIniated_)
  {
    init(stats_);
  }
}

void KvzRTP::stop()
{
  isRunning_ = false;
}

bool KvzRTP::checkSessionID(uint32_t sessionID)
{
  return (uint32_t)peers_.size() >= sessionID && peers_.at(sessionID - 1) != nullptr;
}

bool KvzRTP::addPeer(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  // not being destroyed
  if (destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    printDebug(DEBUG_NORMAL, this, "Adding new peer");

    Peer *peer = new Peer;

    if((uint32_t)peers_.size() >= sessionID && peers_.at(sessionID - 1) == nullptr)
    {
      peers_[sessionID - 1] = peer;
    }
    else
    {
      while((uint32_t)peers_.size() < sessionID - 1)
      {
        peers_.append(nullptr);
      }

      peers_.push_back(peer);
    }

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

std::pair<
  std::shared_ptr<Filter>,
  std::shared_ptr<Filter>
>
KvzRTP::addMediaStream(uint32_t peer, QHostAddress ip, uint16_t src_port,
                                               uint16_t dst_port, QString codec)
{
  Q_ASSERT(checkSessionID(peer));

  // check if the IP addresses start with ::ffff: and remove it, kvzrtp only accepts IPv4 addresses
  if (ip.toString().left(7) == "::ffff:")
  {
    ip = QHostAddress(ip.toString().mid(7));
  }

  rtp_format_t fmt  = typeFromString(codec);
  QString mediaName = QString::number(dst_port);
  DataType realType = NONE;

  switch (fmt)
  {
    case RTP_FORMAT_HEVC:
      mediaName += "_HEVC";
      realType = HEVCVIDEO;
      break;

    case RTP_FORMAT_OPUS:
      mediaName += "_OPUS";
      realType = OPUSAUDIO;
      break;

    case RTP_FORMAT_GENERIC:
      mediaName += "_RAW_AUDIO";
      realType = RAWAUDIO;
      break;
    
    default :
      printDebug(DEBUG_ERROR, this, "Warning: RTP support not implemented for this format");
      mediaName += "_UNKNOWN";
      break;
  }

  if (fmt == RTP_FORMAT_HEVC)
  {
    /* TODO: session->create_media_stream */

    peers_[peer - 1]->video_ip = ip;
    peers_[peer - 1]->video    = new MediaStream;

    peers_[peer - 1]->video->stream =
      new kvz_rtp::media_stream(ip.toString().toStdString(), src_port, dst_port, fmt, 0);

    (void)peers_[peer - 1]->video->stream->init();

    peers_[peer - 1]->video->source = std::shared_ptr<KvzRTPSender>(
      new KvzRTPSender(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        peers_[peer - 1]->video->stream
      )
    );

    peers_[peer - 1]->video->sink = std::shared_ptr<KvzRTPReceiver>(
        new KvzRTPReceiver(
          ip.toString() + "_",
          stats_,
          realType,
          mediaName,
          peers_[peer - 1]->video->stream
        )
    );

    return std::make_pair(peers_[peer - 1]->video->source, peers_[peer - 1]->video->sink);
  }
  else
  {
    peers_[peer - 1]->audio_ip = ip;
    peers_[peer - 1]->audio    = new MediaStream;

    peers_[peer - 1]->audio->stream =
      new kvz_rtp::media_stream(ip.toString().toStdString(), src_port, dst_port, fmt, 0);

    (void)peers_[peer - 1]->video->stream->init();

    peers_[peer - 1]->audio->source = std::shared_ptr<KvzRTPSender>(
      new KvzRTPSender(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        peers_[peer - 1]->audio->stream
      )
    );

    peers_[peer - 1]->audio->sink = std::shared_ptr<KvzRTPReceiver>(
        new KvzRTPReceiver(
          ip.toString() + "_",
          stats_,
          realType,
          mediaName,
          peers_[peer - 1]->audio->stream
        )
    );

    return std::make_pair(peers_[peer - 1]->audio->source, peers_[peer - 1]->audio->sink);
  }

  return std::make_pair(nullptr, nullptr);
}

rtp_format_t KvzRTP::typeFromString(QString type)
{
  std::map<QString, rtp_format_t> xmap = {
      { "pcm",  RTP_FORMAT_GENERIC },
      { "opus", RTP_FORMAT_OPUS },
      { "h265", RTP_FORMAT_HEVC }
  };

  if (xmap.find(type) == xmap.end())
  {
    printDebug(DEBUG_ERROR, this, "Tried to use non-defined codec type in RTPSreamer.");
    return RTP_FORMAT_GENERIC;
  }

  return xmap[type];
}

std::shared_ptr<Filter> KvzRTP::addSendStream(uint32_t peer, QHostAddress ip,
                                              uint16_t dst_port, uint16_t src_port,
                                              QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));
  Q_UNUSED(peer);
  Q_UNUSED(ip);
  Q_UNUSED(dst_port);
  Q_UNUSED(src_port);
  Q_UNUSED(codec);
  Q_UNUSED(rtpNum);

  return nullptr;
}

std::shared_ptr<Filter> KvzRTP::addReceiveStream(uint32_t peer, QHostAddress ip, uint16_t port,
                                                 QString codec, uint8_t rtpNum)
{
  Q_UNUSED(peer);
  Q_UNUSED(ip);
  Q_UNUSED(port);
  Q_UNUSED(codec);
  Q_UNUSED(rtpNum);

  return nullptr;
}

void KvzRTP::removePeer(uint32_t sessionID)
{
  Q_UNUSED(sessionID);

  /* TODO:  */
}

void KvzRTP::removeAllPeers()
{
  for (int i = 0; i < peers_.size(); ++i)
  {
    if (peers_.at(i) != nullptr)
    {
      removePeer(i + 1);
    }
  }
}
