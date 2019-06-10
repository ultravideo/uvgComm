#include "rtpstreamer.h"

#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "common.h"

/* #include <liveMedia.hh> */
/* #include <UsageEnvironment.hh> */
/* #include <GroupsockHelper.hh> */
/* #include <BasicUsageEnvironment.hh> */

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <iostream>

#include "../rtplib/src/util.hh"
#include "../rtplib/src/rtp_opus.hh"

RTPStreamer::RTPStreamer():
  rtp_ctx_(),
  isIniated_(false)
{
}

RTPStreamer::~RTPStreamer()
{
}

void RTPStreamer::init(StatisticsInterface *stats)
{
  stats_ = stats;
  isIniated_ = true;
}

void RTPStreamer::uninit()
{
  removeAllPeers();
  isIniated_ = false;
}

void RTPStreamer::run()
{
  if (!isIniated_)
  {
    init(stats_);
  }
}

void RTPStreamer::stop()
{
  isRunning_ = false;
}

bool RTPStreamer::addPeer(QHostAddress address, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  // not being destroyed
  if (destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    qDebug() << "Adding peer to following IP: " << address.toString();

    Peer *peer = new Peer;

#ifdef _WIN32
      peer->ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());
#else
      peer->ip.s_addr = qToBigEndian(address.toIPv4Address());
#endif
    peer->ip_str = address;
    peer->videoSender = nullptr;
    peer->videoReceiver = nullptr;
    peer->audioSender = nullptr;
    peer->audioReceiver = nullptr;

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

    qDebug() << "RTP streamer: SessionID #" << sessionID << "added to rtp streamer.";

    return true;
  }
  qWarning() <<  "Trying to add peer while RTP was being destroyed.";

  return false;
}

std::shared_ptr<Filter> RTPStreamer::addSendStream(uint32_t peer, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));

  rtp_format_t type = typeFromString(codec);

  if (type == RTP_FORMAT_HEVC)
  {
    if (peers_.at(peer - 1)->videoSender)
    {
      destroySender(peers_.at(peer - 1)->videoSender);
    }

    peers_.at(peer - 1)->videoSender = addSender(peers_.at(peer - 1)->ip_str, port, type, rtpNum);
    return peers_.at(peer - 1)->videoSender->sourcefilter;
  }
  else if (type == RTP_FORMAT_OPUS || type == RTP_FORMAT_GENERIC)
  {
    if (peers_.at(peer - 1)->audioSender)
    {
      destroySender(peers_.at(peer - 1)->audioSender);
    }

    peers_.at(peer - 1)->audioSender = addSender(peers_.at(peer - 1)->ip_str, port, type, rtpNum);
    return peers_.at(peer - 1)->audioSender->sourcefilter;
  }

  return nullptr;
}

std::shared_ptr<Filter> RTPStreamer::addReceiveStream(uint32_t peer, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));

  rtp_format_t type = typeFromString(codec);

  if (type == RTP_FORMAT_HEVC)
  {
    if (peers_.at(peer - 1)->videoReceiver)
    {
      destroyReceiver(peers_.at(peer - 1)->videoReceiver);
    }

    peers_.at(peer - 1)->videoReceiver = addReceiver(peers_.at(peer - 1)->ip_str, port, type, rtpNum);
    return peers_.at(peer - 1)->videoReceiver->sink;
  }
  else if (type == RTP_FORMAT_OPUS || type == RTP_FORMAT_GENERIC)
  {
    if (peers_.at(peer - 1)->audioReceiver)
    {
      destroyReceiver(peers_.at(peer - 1)->audioReceiver);
    }

    peers_.at(peer - 1)->audioReceiver = addReceiver(peers_.at(peer - 1)->ip_str, port, type, rtpNum);
    return peers_.at(peer - 1)->audioReceiver->sink;
  }

  return nullptr;
}

void RTPStreamer::removeSendVideo(uint32_t sessionID)
{
  if (peers_.at(sessionID - 1) && peers_.at(sessionID)->videoSender)
  {
      destroySender(peers_.at(sessionID - 1)->videoSender);
      peers_[sessionID - 1]->videoSender = nullptr;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to remove videoSender that doesn't exist!");
  }
}

void RTPStreamer::removeSendAudio(uint32_t sessionID)
{
  if (peers_.at(sessionID - 1) && peers_.at(sessionID)->audioSender)
  {
      destroySender(peers_.at(sessionID - 1)->audioSender);
      peers_[sessionID - 1]->audioSender = nullptr;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to remove audioSender that doesn't exist!");
  }
}

void RTPStreamer::removeReceiveVideo(uint32_t sessionID)
{
  if (peers_.at(sessionID - 1) && peers_.at(sessionID)->videoReceiver)
  {
      destroyReceiver(peers_.at(sessionID - 1)->videoReceiver);
      peers_[sessionID - 1]->videoReceiver = nullptr;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to remove videoReceiver that doesn't exist!");
  }
}

void RTPStreamer::removeReceiveAudio(uint32_t sessionID)
{
  if (peers_.at(sessionID - 1) && peers_.at(sessionID)->audioReceiver)
  {
      destroyReceiver(peers_.at(sessionID - 1)->audioReceiver);
      peers_[sessionID - 1]->audioReceiver = nullptr;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to remove audioReceiver that doesn't exist!");
  }
}

void RTPStreamer::removePeer(uint32_t sessionID)
{
  qDebug() << "Removing peer" << sessionID << "from RTPStreamer";

  if (peers_.at(sessionID - 1) != nullptr)
  {
    if (peers_.at(sessionID -1 )->audioSender)
      destroySender(peers_.at(sessionID - 1)->audioSender);

    if (peers_.at(sessionID - 1)->videoSender)
      destroySender(peers_.at(sessionID - 1)->videoSender);

    if (peers_.at(sessionID - 1)->audioReceiver)
      destroyReceiver(peers_.at(sessionID - 1)->audioReceiver);

    if (peers_.at(sessionID - 1)->videoReceiver)
      destroyReceiver(peers_.at(sessionID - 1)->videoReceiver);

    delete peers_.at(sessionID - 1);
    peers_[sessionID - 1] = nullptr;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to destroy an already freed peer",
              { "SessionID" }, { QString(sessionID) });
  }
}

void RTPStreamer::removeAllPeers()
{
  for (int i = 0; i < peers_.size(); ++i)
  {
    if (peers_.at(i) != nullptr)
    {
      removePeer(i + 1);
    }
  }
}

void RTPStreamer::destroySender(Sender *sender)
{
  Q_ASSERT(sender);

  if (sender)
  {
    printDebug(DEBUG_NORMAL, this, DC_REMOVE_MEDIA, "Destroying sender");

    if (rtp_ctx_.destroy_writer(sender->writer) != RTP_OK)
    {
      printDebug(DEBUG_ERROR, this, DC_REMOVE_MEDIA, "Failed to destroy RTP Writer");
    }

    delete sender;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to delete sender a second time");
  }
}
void RTPStreamer::destroyReceiver(Receiver *recv)
{
  Q_ASSERT(recv);

  if (recv)
  {
    printDebug(DEBUG_NORMAL, this, DC_REMOVE_MEDIA, "Destroying receiver");

    if (rtp_ctx_.destroy_reader(recv->reader) != RTP_OK)
    {
      printDebug(DEBUG_ERROR, this, DC_REMOVE_MEDIA, "Failed to destroy RTP Reader");
    }

    delete recv;
  }
  else
  {
    printDebug(DEBUG_WARNING, this, DC_REMOVE_MEDIA, "Tried to delete receiver a second time");
  }
}

bool RTPStreamer::checkSessionID(uint32_t sessionID)
{
  return (uint32_t)peers_.size() >= sessionID && peers_.at(sessionID - 1) != nullptr;
}

RTPStreamer::Sender *RTPStreamer::addSender(QHostAddress ip, uint16_t port, rtp_format_t type, uint8_t rtpNum)
{
  qDebug() << "Iniating send RTP/RTCP stream to port:" << port << "With type:" << type;

  Sender *sender = new Sender;

  /* TODO: source port (1337 is not valid!) */

  sender->writer = rtp_ctx_.create_writer(ip.toString().toStdString(), port, 1337, type);
  sender->writer->start();

  // TODO is this necessary????
  QString mediaName = QString::number(port);
  DataType realType = NONE;
  kvz_rtp::opus::opus_config *config;

  switch (type)
  {
    case RTP_FORMAT_HEVC:
      mediaName += "_HEVC";
      realType = HEVCVIDEO;
      break;

    case RTP_FORMAT_OPUS:
      config = new kvz_rtp::opus::opus_config;

      config->channels = 2;
      config->samplerate = 48000;
      config->config_number = 15; // Hydrib | FB | 20 ms
      sender->writer->set_config(config);

      mediaName += "_OPUS";
      realType = OPUSAUDIO;
      break;

    case RTP_FORMAT_GENERIC:
      mediaName += "_RAW_AUDIO";
      realType = RAWAUDIO;
      break;

    default :
      qWarning() << "Warning: RTP support not implemented for this format";
      mediaName += "_UNKNOWN";
      break;
  }

  sender->sourcefilter = std::shared_ptr<FramedSourceFilter>(
      new FramedSourceFilter(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        sender->writer
      )
  );

  return sender;
}

RTPStreamer::Receiver *RTPStreamer::addReceiver(QHostAddress ip, uint16_t port, rtp_format_t type, uint8_t rtpNum)
{
  qDebug() << "Iniating send RTP/RTCP stream to port:" << port << "With type:" << type;

  Receiver *receiver = new Receiver;
  receiver->reader   = rtp_ctx_.create_reader(ip.toString().toStdString(), port, type);
  receiver->reader->start();

  /* unsigned int estimatedSessionBandwidth = 10000; // in kbps; for RTCP b/w share */

  // todo: negotiate payload number TODO mitä??
  QString mediaName = QString::number(port);
  DataType realType = NONE;

  switch(type)
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
      qWarning() << "Warning: RTP support not implemented for this format";
      break;
  }

  // shared_ptr which does not release
  receiver->sink = std::shared_ptr<RTPSinkFilter>(
      new RTPSinkFilter(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        receiver->reader
      )
  );

  return receiver;
}

rtp_format_t RTPStreamer::typeFromString(QString type)
{
  std::map<QString, rtp_format_t> xmap = {
      { "pcm",  RTP_FORMAT_GENERIC },
      { "opus", RTP_FORMAT_OPUS },
      { "h265", RTP_FORMAT_HEVC }
  };

  if (xmap.find(type) == xmap.end())
  {
    qDebug() << "ERROR: Tried to use non-defined codec type in RTPSreamer.";
    return RTP_FORMAT_GENERIC;
  }

  return xmap[type];
}
