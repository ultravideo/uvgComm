#include "media/delivery/irtpstreamer.h"
#include "media/delivery/kvzrtp/kvzrtp.h"
#include "kvzrtp.h"
#include "kvzrtpsender.h"
#include "kvzrtpreceiver.h"
#include "common.h"

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <kvzrtp/rtp_opus.hh>

#include <iostream>

KvzRTP::KvzRTP():
  rtp_ctx_(),
  isIniated_(false)
{
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

bool KvzRTP::addPeer(uint32_t sessionID, QHostAddress video_ip, QHostAddress audio_ip)
{
  Q_ASSERT(sessionID != 0);

  // check if the IP addresses start with ::ffff: and remove it, kvzrtp only accepts IPv4 addresses
  if (video_ip.toString().left(7) == "::ffff:")
  {
    video_ip = QHostAddress(video_ip.toString().mid(7));
  }

  if (audio_ip.toString().left(7) == "::ffff:")
  {
    audio_ip = QHostAddress(audio_ip.toString().mid(7));
  }

  // not being destroyed
  if (destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    printDebug(DEBUG_NORMAL, this, DC_MEDIA, "Adding new peer",
        { "Video IP", "Audio IP" }, { video_ip.toString(), audio_ip.toString(); });

    Peer *peer = new Peer;

    peer->video_ip = video_ip;
    peer->audio_ip = audio_ip;
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

    printDebug(DEBUG_NORMAL, this, DC_MEDIA, "RTP streamer", { "SessionID" }, { sessionID });

    return true;
  }
  printDebug(DEBUG_WARNING, this, DC_MEDIA, "Trying to add peer while RTP was being destroyed.");

  return false;
}

std::shared_ptr<Filter> KvzRTP::addSendStream(uint32_t peer, QHostAddress ip, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));
  Q_UNUSED(peer);

  KvzRTP::Sender *sender = addSender(ip, port, typeFromString(codec), rtpNum);

  if (sender == nullptr)
  {
    return nullptr;
  }

  senders_.push_back(sender);
  return sender->sourcefilter;
}

std::shared_ptr<Filter> KvzRTP::addReceiveStream(uint32_t peer, QHostAddress ip, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));
  Q_UNUSED(peer);

  KvzRTP::Receiver *receiver = addReceiver(ip, port, typeFromString(codec), rtpNum);

  if (receiver == nullptr)
  {
    return nullptr;
  }

  receivers_.push_back(receiver);
  return receiver->sink;
}

std::shared_ptr<Filter> KvzRTP::addSendStream(uint32_t peer, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));

  rtp_format_t type = typeFromString(codec);

  if (type == RTP_FORMAT_HEVC)
  {
    if (peers_.at(peer - 1)->videoSender)
    {
      destroySender(peers_.at(peer - 1)->videoSender);
    }

    peers_.at(peer - 1)->videoSender = addSender(peers_.at(peer - 1)->video_ip, port, type, rtpNum);
    return peers_.at(peer - 1)->videoSender->sourcefilter;
  }
  else if (type == RTP_FORMAT_OPUS || type == RTP_FORMAT_GENERIC)
  {
    if (peers_.at(peer - 1)->audioSender)
    {
      destroySender(peers_.at(peer - 1)->audioSender);
    }

    peers_.at(peer - 1)->audioSender = addSender(peers_.at(peer - 1)->audio_ip, port, type, rtpNum);
    return peers_.at(peer - 1)->audioSender->sourcefilter;
  }

  return nullptr;
}

std::shared_ptr<Filter> KvzRTP::addReceiveStream(uint32_t peer, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(checkSessionID(peer));

  rtp_format_t type = typeFromString(codec);

  if (type == RTP_FORMAT_HEVC)
  {
    if (peers_.at(peer - 1)->videoReceiver)
    {
      destroyReceiver(peers_.at(peer - 1)->videoReceiver);
    }

    peers_.at(peer - 1)->videoReceiver = addReceiver(peers_.at(peer - 1)->video_ip, port, type, rtpNum);
    return peers_.at(peer - 1)->videoReceiver->sink;
  }
  else if (type == RTP_FORMAT_OPUS || type == RTP_FORMAT_GENERIC)
  {
    if (peers_.at(peer - 1)->audioReceiver)
    {
      destroyReceiver(peers_.at(peer - 1)->audioReceiver);
    }

    peers_.at(peer - 1)->audioReceiver = addReceiver(peers_.at(peer - 1)->audio_ip, port, type, rtpNum);
    return peers_.at(peer - 1)->audioReceiver->sink;
  }

  return nullptr;
}

void KvzRTP::removeSendVideo(uint32_t sessionID)
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

void KvzRTP::removeSendAudio(uint32_t sessionID)
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

void KvzRTP::removeReceiveVideo(uint32_t sessionID)
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

void KvzRTP::removeReceiveAudio(uint32_t sessionID)
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

void KvzRTP::removePeer(uint32_t sessionID)
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

void KvzRTP::destroySender(Sender *sender)
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
void KvzRTP::destroyReceiver(Receiver *recv)
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

bool KvzRTP::checkSessionID(uint32_t sessionID)
{
  return (uint32_t)peers_.size() >= sessionID && peers_.at(sessionID - 1) != nullptr;
}

KvzRTP::Sender *KvzRTP::addSender(QHostAddress ip, uint16_t port, rtp_format_t type, uint8_t rtpNum)
{
  printDebug(DEBUG_NORMAL, this, DC_MEDIA, "Iniating send RTP/RTCP stream", { "Port", "Type" }, { port, type });

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
      printDebug(DEBUG_ERROR, this, DC_MEDIA, "Warning: RTP support not implemented for this format");
      mediaName += "_UNKNOWN";
      break;
  }

  sender->sourcefilter = std::shared_ptr<KvzRTPSender>(
      new KvzRTPSender(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        sender->writer
      )
  );

  return sender;
}

KvzRTP::Receiver *KvzRTP::addReceiver(QHostAddress ip, uint16_t port, rtp_format_t type, uint8_t rtpNum)
{
  printDebug(DEBUG_NORMAL, this, DC_MEDIA, "Iniating receive RTP/RTCP stream", { "Port", "Type" }, { port, type });

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
      printDebug(DEBUG_ERROR, this, DC_MEDIA, "Warning: RTP support not implemented for this format");
      mediaName += "_UNKNOWN";
      break;
  }

  // shared_ptr which does not release
  receiver->sink = std::shared_ptr<KvzRTPReceiver>(
      new KvzRTPReceiver(
        ip.toString() + "_",
        stats_,
        realType,
        mediaName,
        receiver->reader
      )
  );

  return receiver;
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
    printDebug(DEBUG_ERROR, this, DC_MEDIA, "Tried to use non-defined codec type in RTPSreamer.");
    return RTP_FORMAT_GENERIC;
  }

  return xmap[type];
}
