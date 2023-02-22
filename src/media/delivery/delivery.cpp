#include "delivery.h"
#include "uvgrtpsender.h"
#include "uvgrtpreceiver.h"

#include "common.h"
#include "logger.h"
#include "settingskeys.h"

#include <QtEndian>
#include <QHostInfo>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFuture>
#include <uvgrtp/lib.hh>

#include <iostream>

Delivery::Delivery():
  rtp_ctx_(new uvg_rtp::context),
  stats_(nullptr),
  hwResources_(nullptr)
{}


Delivery::~Delivery()
{}


void Delivery::init(StatisticsInterface *stats,
                    std::shared_ptr<ResourceAllocator> hwResources)
{
  stats_ = stats;
  hwResources_ = hwResources;

  uvgrtp::context ctx;
  if (!ctx.crypto_enabled() && settingEnabled(SettingsKey::sipSRTP))
  {
    Logger::getLogger()->printWarning(this, "uvgRTP does not have Crypto++ included. "
                                            "Cannot encrypt media traffic");
    emit handleNoEncryption();
  }
}


void Delivery::uninit()
{
  removeAllPeers();
}

bool Delivery::addSession(uint32_t sessionID,
                          QString peerAddressType, QString peerAddress,
                          QString localAddressType, QString localAddress)
{
  Q_ASSERT(sessionID != 0);

  if (peers_.find(sessionID) == peers_.end() || peers_[sessionID] == nullptr)
  {
    peers_[sessionID] = std::shared_ptr<Peer> (new Peer());
  }

  QString connection = localAddress + "<->" + peerAddress;

  uint32_t index = 0;
  if (findSession(sessionID, index, localAddress, peerAddress))
  {
    Logger::getLogger()->printNormal(this, "Reusing existing session", "Connection", connection);
    return true;
  }
  else if (peerAddressType == "IP4" && localAddressType == "IP4")
  {
    Logger::getLogger()->printNormal(this, "Adding a new session for peer since we use different route",
                                      "Connection", connection);

    ipv6to4(peerAddress);
    peers_[sessionID]->sessions.push_back({rtp_ctx_->create_session(peerAddress.toStdString(), localAddress.toStdString()),
                                           localAddress, peerAddress, {}, false});
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Tried to use address type "
                                                  "that has not been implemented yet!");
    return false;

  }

  return true;
}


void Delivery::parseCodecString(QString codec, rtp_format_t& fmt,
                                DataType& type, QString& mediaName)
{
  std::map<QString, rtp_format_t> xmap = {
      { "PCMU", RTP_FORMAT_GENERIC },
      { "opus", RTP_FORMAT_OPUS },
      { "H264", RTP_FORMAT_H264 },
      { "H265", RTP_FORMAT_H265 },
      { "H266", RTP_FORMAT_H266 }
  };

  if (xmap.find(codec) == xmap.end())
  {
    Logger::getLogger()->printError(this, "Tried to use non-defined codec type in uvgRTP.");
    type = DT_NONE;
    return;
  }

  fmt  = xmap[codec];
  mediaName = "";
  type = DT_NONE;

  switch (fmt)
  {
    case RTP_FORMAT_H264:
      mediaName = "AVC";
      type = DT_HEVCVIDEO;
      break;
    case RTP_FORMAT_H265:
      mediaName = "HEVC";
      type = DT_HEVCVIDEO;
      break;
    case RTP_FORMAT_H266:
      mediaName = "VVC";
      type = DT_HEVCVIDEO;
      break;
    case RTP_FORMAT_OPUS:
      mediaName = "OPUS";
      type = DT_OPUSAUDIO;
      break;

    case RTP_FORMAT_GENERIC:
      mediaName+= "PCMU";
      type = DT_RAWAUDIO;
      break;

    default :
      Logger::getLogger()->printError(this, "RTP support not implemented for this format");
      mediaName += "UNKNOWN";
      break;
  }
}


std::shared_ptr<Filter> Delivery::addSendStream(uint32_t sessionID,
                                                QString localAddress, QString remoteAddress,
                                                uint16_t localPort, uint16_t peerPort,
                                                QString codec, uint8_t rtpNum)
{
  Q_UNUSED(rtpNum); // TODO in uvgRTP

  Logger::getLogger()->printNormal(this, "Creating uvgRTP send stream",
                                   "Path", QString::number(localPort) + " -> " +
                                   remoteAddress + ":" + QString::number(peerPort));

  rtp_format_t fmt = RTP_FORMAT_GENERIC;
  DataType type = DT_NONE;
  QString mediaName = "";

  parseCodecString(codec, fmt, type, mediaName);

  uint32_t sessionIndex = 0;
  if (!findSession(sessionID, sessionIndex, localAddress, remoteAddress))
  {
    Logger::getLogger()->printError(this, "Did not find session");
    return nullptr;
  }

  if (!initializeStream(sessionID, peers_[sessionID]->sessions.at(sessionIndex),
                        localPort, peerPort, fmt))
  {
    Logger::getLogger()->printError(this, "Failed to initialize stream");
    return nullptr;
  }

  // create filter if it does not exist
  if (peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->sender == nullptr)
  {
    Logger::getLogger()->printNormal(this, "Creating sender filter");

    peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->sender =
        std::shared_ptr<UvgRTPSender>(new UvgRTPSender(sessionID,
                                                       remoteAddress + ":" + QString::number(peerPort),
                                                       stats_,
                                                       hwResources_,
                                                       type,
                                                       mediaName,
                                                       peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->stream));

    connect(
      peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->sender.get(),
      &UvgRTPSender::zrtpFailure,
      this,
      &Delivery::handleZRTPFailure);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing sender filter");
  }

  return peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->sender;
}

std::shared_ptr<Filter> Delivery::addReceiveStream(uint32_t sessionID,
                                                   QString localAddress, QString remoteAddress,
                                                   uint16_t localPort, uint16_t peerPort,
                                                   QString codec, uint8_t rtpNum)
{
  Q_UNUSED(rtpNum); // TODO in uvgRTP

  Logger::getLogger()->printNormal(this, "Creating uvgRTP receive stream",
                                   "Path", localAddress + ":" +
                                   QString::number(localPort) +
                                   " <- " +
                                   QString::number(peerPort));

  rtp_format_t fmt = RTP_FORMAT_GENERIC;
  DataType type = DT_NONE;
  QString mediaName = "";

  parseCodecString(codec, fmt, type, mediaName);

  uint32_t sessionIndex = 0;
  if (!findSession(sessionID, sessionIndex, localAddress, remoteAddress))
  {
    Logger::getLogger()->printError(this, "Did not find session");
    return nullptr;
  }

  if (!initializeStream(sessionID, peers_[sessionID]->sessions.at(sessionIndex),
                        localPort, peerPort, fmt))
  {
    return nullptr;
  }

  // create filter if it does not exist
  if (peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->receiver == nullptr)
  {
    Logger::getLogger()->printNormal(this, "Creating receiver filter", "Interface",
                                     localAddress + ":" + QString::number(localPort));
    peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->receiver = std::shared_ptr<UvgRTPReceiver>(
        new UvgRTPReceiver(
          sessionID,
          localAddress + ":" + QString::number(localPort),
          stats_,
          hwResources_,
          type,
          mediaName,
          peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->stream
        )
    );

    connect(
      peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->receiver.get(),
      &UvgRTPReceiver::zrtpFailure,
      this,
      &Delivery::handleZRTPFailure);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing RTP receiver");
  }

  return peers_[sessionID]->sessions.at(sessionIndex).streams[localPort]->receiver;
}


bool Delivery::initializeStream(uint32_t sessionID,
                                DeliverySession& session,
                                uint16_t localPort, uint16_t peerPort,
                                rtp_format_t fmt)
{
  bool success = true;
  // add peer if it does not exist
  if (peers_.find(sessionID) == peers_.end())
  {
    return false;
  }

  // create mediastream if it does not exist
  if (session.streams.find(localPort) == session.streams.end())
  {
    Logger::getLogger()->printNormal(this, "Creating a new uvgRTP mediastream");
    success = addMediaStream(sessionID, session,
                             localPort, peerPort, fmt, session.dhSelected);
    if (success)
    {
      session.dhSelected = true;
    }
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Reusing existing uvgRTP mediastream");
  }

  return success;
}


bool Delivery::addMediaStream(uint32_t sessionID, DeliverySession &session,
                              uint16_t localPort, uint16_t peerPort,
                              rtp_format_t fmt, bool dhSelected)
{
  if (peers_.find(sessionID) == peers_.end())
  {
    return false;
  }

  Logger::getLogger()->printNormal(this, "Creating mediastream");

  // This makes the uvgRTP keep the firewall open even if the other side is not sending media
  int flags = RCE_HOLEPUNCH_KEEPALIVE;

  if (fmt == RTP_FORMAT_H264 ||
      fmt == RTP_FORMAT_H265 ||
      fmt == RTP_FORMAT_H266)
  {
    //flags |= RCE_FRAME_RATE;
    flags |= RCE_PACE_FRAGMENT_SENDING;
  }

  // enable encryption if it works
    uvgrtp::context ctx;
  if (ctx.crypto_enabled() && settingEnabled(SettingsKey::sipSRTP))
  {
    Logger::getLogger()->printNormal(this, "Encryption enabled");

    // enable srtp + zrtp
    flags |= RCE_SRTP;
    flags |= RCE_SRTP_KMNGMNT_ZRTP;

    if (!dhSelected)
    {
      flags |= RCE_ZRTP_DIFFIE_HELLMAN_MODE;
    }
    else
    {
      flags |= RCE_ZRTP_MULTISTREAM_MODE;
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "No media encryption");
  }

  QFuture<uvg_rtp::media_stream *> futureRes =
    QtConcurrent::run([=](uvg_rtp::session *session, uint16_t local, uint16_t peer,
          rtp_format_t fmt, int flags)
    {
        return session->create_stream(local, peer, fmt, flags);
    },
    session.session, localPort, peerPort, fmt, flags);

  // check if there already exists a media session and overwrite
  if (session.streams.find(localPort) != session.streams.end() &&
      session.streams[localPort] != nullptr)
  {
    Logger::getLogger()->printProgramWarning(this, "Existing mediastream detected. Overwriting."
                              " Will cause a crash if previous filters are attached to filtergraph.");
    removeMediaStream(sessionID, session, localPort);
  }

  // actually create the mediastream
  session.streams[localPort] = new MediaStream;
  session.streams[localPort]->stream = futureRes;

  return true;
}


void Delivery::removeMediaStream(uint32_t sessionID, DeliverySession &session, uint16_t localPort)
{
  Logger::getLogger()->printNormal(this, "Removing mediastream");

  session.session->destroy_stream(session.streams[localPort]->stream);
  delete session.streams[localPort];
  session.streams[localPort] = nullptr;
  session.streams.erase(localPort);
}


void Delivery::removePeer(uint32_t sessionID)
{
  if (peers_.find(sessionID) != peers_.end())
  {
    for (auto& session : peers_[sessionID]->sessions)
    {
      std::vector<uint16_t> streams;

      // take all keys so we wont get iterator errors
      for (auto& stream : session.streams)
      {
        streams.push_back(stream.first);
      }

      // delete all streams
      for (auto& stream : streams)
      {
        removeMediaStream(sessionID, session, stream);
      }

      // delete session
      if (session.session)
      {
        // TODO: uvgRTP free_session
        delete session.session;
        session.session = nullptr;
      }
    }
    peers_.erase(sessionID);
  }
}


void Delivery::removeAllPeers()
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

void Delivery::ipv6to4(QHostAddress& address)
{
  // check if the IP addresses start with ::ffff: and
  // remove it, uvgrtp only accepts IPv4 addresses
  if (address.toString().left(7) == "::ffff:")
  {
    address = QHostAddress(address.toString().mid(7));
  }
}

void Delivery::ipv6to4(QString &address)
{
  // check if the IP addresses start with ::ffff: and
  // remove it, uvgrtp only accepts IPv4 addresses
  if (address.left(7) == "::ffff:")
  {
    address = address.mid(7);
  }
}


bool Delivery::findSession(uint32_t sessionID, uint32_t& outIndex,
                           QString localAddress, QString remoteAddress)
{
  bool sessionExists = false;
  for (uint32_t index = 0; index < peers_[sessionID]->sessions.size(); ++index)
  {
    if (peers_[sessionID]->sessions.at(index).localAddress == localAddress &&
        peers_[sessionID]->sessions.at(index).peerAddress == remoteAddress)
    {
      sessionExists = true;
      outIndex = index;
      break;
    }
  }


  return sessionExists;
}
