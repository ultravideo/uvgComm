#include "delivery.h"
#include "uvgrtpsender.h"
#include "uvgrtpreceiver.h"

#include "common.h"
#include "logger.h"
#include "settingskeys.h"
#include "udprelay.h"
#include "uvgrelay.h"
#include "udpsender.h"
#include "udpreceiver.h"

#include <QtEndian>
#include <QHostInfo>
#include <QCoreApplication>
#include <QThread>

#include <uvgrtp/lib.hh>

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
  else
  {
    Logger::getLogger()->printNormal(this, "Adding a new session for peer since route is not recognized",
                                      "Connection", connection);

    peers_[sessionID]->sessions.push_back({rtp_ctx_->create_session(peerAddress.toStdString(), localAddress.toStdString()),
                                           localAddress, peerAddress, {}, {}, false});
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


std::shared_ptr<Filter> Delivery::addRTPSendStream(uint32_t sessionID,
                                                QString localAddress, QString remoteAddress,
                                                uint16_t localPort, uint16_t peerPort,
                                                QString codec, uint8_t rtpNum,
                                                uint32_t localSSRC, uint32_t remoteSSRC)
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
                        localPort, peerPort, fmt, localSSRC, remoteSSRC, false))
  {
    Logger::getLogger()->printError(this, "Failed to initialize stream");
    return nullptr;
  }

  // create filter if it does not exist
  if (peers_[sessionID]->sessions.at(sessionIndex).outgoingStreams[localSSRC]->sender == nullptr)
  {
    Logger::getLogger()->printNormal(this, "Creating sender filter");

    peers_[sessionID]->sessions.at(sessionIndex).outgoingStreams[localSSRC]->sender =
        std::shared_ptr<UvgRTPSender>(new UvgRTPSender(sessionID,
                                                       remoteAddress + ":" + QString::number(peerPort),
                                                       stats_,
                                                       hwResources_,
                                                       type,
                                                       mediaName,
                                                       localSSRC,
                                                       remoteSSRC,
                                                       peers_[sessionID]->sessions.at(sessionIndex).outgoingStreams[localSSRC]->ms,
                                                       peers_[sessionID]->sessions.at(sessionIndex).outgoingStreams[localSSRC]->runZRTP));

    connect(
      peers_[sessionID]->sessions.at(sessionIndex).outgoingStreams[localSSRC]->sender.get(),
      &UvgRTPSender::zrtpFailure,
      this,
      &Delivery::handleZRTPFailure);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing sender filter",
                                     "SessionID", QString::number(sessionID));
  }

  return peers_[sessionID]->sessions.at(sessionIndex).outgoingStreams[localSSRC]->sender;
}


std::shared_ptr<Filter> Delivery::addUDPSendStream(uint32_t sessionID,
                                         QString localAddress, QString remoteAddress,
                                         uint16_t localPort, uint16_t peerPort, uint32_t remoteSSRC)
{
  if (udpSenders_.find(remoteSSRC) == udpSenders_.end())
  {
    Logger::getLogger()->printNormal(this, "Creating a new UDP sender",
                                     "Remote SSRC", QString::number(remoteSSRC));
    std::shared_ptr<RelayInterface> relay = getUDPRelay(localAddress, localPort);

    QString id = remoteAddress + ":" + QString::number(peerPort);

    udpSenders_[remoteSSRC] = std::shared_ptr<UDPSender>(new UDPSender(id, stats_,  hwResources_,
                                                                       remoteAddress.toStdString(),
                                                                       peerPort, relay));
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing UDP sender");
  }

  return udpSenders_[remoteSSRC];
}


std::shared_ptr<Filter> Delivery::addRTPReceiveStream(uint32_t sessionID,
                                                   QString localAddress, QString remoteAddress,
                                                   uint16_t localPort, uint16_t peerPort,
                                                   QString codec, uint8_t rtpNum,
                                                   uint32_t localSSRC, uint32_t remoteSSRC)
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
                        localPort, peerPort, fmt, localSSRC, remoteSSRC, true))
  {
    return nullptr;
  }

  // create filter if it does not exist
  if (peers_[sessionID]->sessions.at(sessionIndex).incomingStreams.find(remoteSSRC) ==
      peers_[sessionID]->sessions.at(sessionIndex).incomingStreams.end() ||
      peers_[sessionID]->sessions.at(sessionIndex).incomingStreams[remoteSSRC]->receiver == nullptr)
  {
    Logger::getLogger()->printNormal(this, "Creating receiver filter", "Interface",
                                     localAddress + ":" + QString::number(localPort));

    peers_[sessionID]->sessions.at(sessionIndex).incomingStreams[remoteSSRC]->receiver = std::shared_ptr<UvgRTPReceiver>(
        new UvgRTPReceiver(
          sessionID,
          localAddress + ":" + QString::number(localPort),
          stats_,
          hwResources_,
          type,
          mediaName,
          localSSRC,
          remoteSSRC,
          peers_[sessionID]->sessions.at(sessionIndex).incomingStreams[remoteSSRC]->ms,
          peers_[sessionID]->sessions.at(sessionIndex).incomingStreams[remoteSSRC]->runZRTP
        )
    );

    connect(
      peers_[sessionID]->sessions.at(sessionIndex).incomingStreams[remoteSSRC]->receiver.get(),
      &UvgRTPReceiver::zrtpFailure,
      this,
      &Delivery::handleZRTPFailure);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing RTP receiver");
  }

  return peers_[sessionID]->sessions.at(sessionIndex).incomingStreams[remoteSSRC]->receiver;
}


std::shared_ptr<Filter> Delivery::addUDPReceiveStream(uint32_t sessionID,
                                                      QString localAddress, uint16_t localPort,
                                                      uint32_t remoteSSRC)
{
  if (udpReceivers_.find(remoteSSRC) == udpReceivers_.end())
  {
    Logger::getLogger()->printNormal(this, "Creating new UDP receiver", "Path",
                                     localAddress + ":" + QString::number(localPort));

    std::shared_ptr<RelayInterface> relay = getUDPRelay(localAddress, localPort);
    QString id = localAddress + ":" + QString::number(localPort);
    udpReceivers_[remoteSSRC] = std::shared_ptr<UDPReceiver>(new UDPReceiver(id, stats_, hwResources_));
    relay->registerRTPReceiver(remoteSSRC, udpReceivers_[remoteSSRC]);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing UDP receiver");
  }

  return udpReceivers_[remoteSSRC];
}


std::shared_ptr<RelayInterface> Delivery::getUDPRelay(QString localAddress, uint16_t localPort)
{
  QString relayKey = localAddress + ":" + QString::number(localPort);

  if (relays_.find(relayKey) == relays_.end())
  {
    Logger::getLogger()->printNormal(this, "Creating new UDP relay", "Local socket", relayKey);

    bool customRelay = true;

    if (customRelay)
    {
      Logger::getLogger()->printNormal(this, "Using custom UDP relay", "Local socket", relayKey);
      relays_[relayKey] = std::shared_ptr<RelayInterface>(new UVGRelay(localAddress.toStdString(), localPort));
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Using Qts UDP relay", "Local socket", relayKey);
      relays_[relayKey] = std::shared_ptr<RelayInterface>(new UDPRelay(localAddress.toStdString(), localPort));
    }

    relays_[relayKey]->start();
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing UDP relay", "Path", relayKey);
  }

  return relays_[relayKey];
}


bool Delivery::initializeStream(uint32_t sessionID,
                                DeliverySession& session,
                                uint16_t localPort, uint16_t peerPort,
                                rtp_format_t fmt, uint32_t localSSRC, uint32_t remoteSSRC, bool recv)
{
  bool success = true;
  // add peer if it does not exist
  if (peers_.find(sessionID) == peers_.end())
  {
    return false;
  }

  if (recv)
  {
    // create mediastream if it does not exist
    if (session.incomingStreams.find(remoteSSRC) == session.incomingStreams.end())
    {
      Logger::getLogger()->printNormal(this, "Creating a new uvgRTP mediastream");
      success = addMediaStream(sessionID, session,
                               localPort, peerPort, fmt, session.dhSelected,
                               localSSRC, remoteSSRC, recv);
      if (success)
      {
        session.dhSelected = true;
      }
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Reusing existing uvgRTP mediastream");
    }
  }
  else
  {
    // create mediastream if it does not exist
    if (session.outgoingStreams.find(localSSRC) == session.outgoingStreams.end())
    {
      Logger::getLogger()->printNormal(this, "Creating a new uvgRTP mediastream");
      success = addMediaStream(sessionID, session,
                               localPort, peerPort, fmt, session.dhSelected,
                               localSSRC, remoteSSRC, recv);
      if (success)
      {
        session.dhSelected = true;
      }
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Reusing existing uvgRTP mediastream");
    }
  }

  return success;
}


bool Delivery::addMediaStream(uint32_t sessionID, DeliverySession &session,
                              uint16_t localPort, uint16_t peerPort,
                              rtp_format_t fmt, bool dhSelected,
                              uint32_t localSSRC, uint32_t remoteSSRC, bool recv)
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

  flags |= RCE_RTCP;
  flags |= RCE_RTCP_MUX;

  bool runZRTP = false;

  // enable encryption if it works
    uvgrtp::context ctx;
  if (ctx.crypto_enabled() && settingEnabled(SettingsKey::sipSRTP))
  {
    Logger::getLogger()->printNormal(this, "Encryption enabled");

    // enable SRTP
    flags |= RCE_SRTP;

    if (runZRTP)
    {
      // zrtp will always be performed explicitly
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
      flags |= RCE_SRTP_KMNGMNT_USER | RCE_SRTP_KEYSIZE_256;
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "No media encryption");
  }

  uvg_rtp::media_stream* stream = session.session->create_stream(localPort, peerPort, fmt, flags, localSSRC);

  if (!runZRTP && ctx.crypto_enabled() && settingEnabled(SettingsKey::sipSRTP))
  {
    constexpr int KEY_SIZE = 256;
    constexpr int KEY_SIZE_BYTES = KEY_SIZE / 8;
    constexpr int SALT_SIZE = 112;
    constexpr int SALT_SIZE_BYTES = SALT_SIZE / 8;
    uint8_t key[KEY_SIZE_BYTES] = {0};
    uint8_t salt[SALT_SIZE_BYTES] = {0};

    // initialize SRTP key and salt with dummy values
    for (int i = 0; i < KEY_SIZE_BYTES; ++i)
      key[i] = i;

    for (int i = 0; i < SALT_SIZE_BYTES; ++i)
      salt[i] = i * 2;

    stream->add_srtp_ctx(key, salt);
  }

  if (recv)
  {
    // check if there already exists a media session and overwrite
    if (session.incomingStreams.find(remoteSSRC) != session.incomingStreams.end()
        && session.incomingStreams[remoteSSRC] != nullptr) {
      Logger::getLogger()->printProgramWarning(this, "Existing mediastream detected. Overwriting. "
                                                     "Will cause a crash if previous filters "
                                                     "are attached to filtergraph.");
      removeRecvStream(sessionID, session, remoteSSRC);
    }

    // actually create the mediastream
    session.incomingStreams[remoteSSRC] = new RecvStream;
    session.incomingStreams[remoteSSRC]->ms = stream;
    session.incomingStreams[remoteSSRC]->runZRTP = runZRTP;
    session.incomingStreams[remoteSSRC]->receiver = nullptr;

    if (remoteSSRC != 0)
    {
      stream->configure_ctx(RCC_REMOTE_SSRC, remoteSSRC);
    }

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Created mediastream for receiving",
                                     {"SessionID","RemoteSSRC","IncomingCount"},
                                     {QString::number(sessionID), QString::number(remoteSSRC), QString::number(session.incomingStreams.size())});
  }
  else
  {
    // check if there already exists a media session and overwrite
    if (session.outgoingStreams.find(localSSRC) != session.outgoingStreams.end()
        && session.outgoingStreams[localSSRC] != nullptr) {
      Logger::getLogger()->printProgramWarning(this, "Existing mediastream detected. Overwriting. "
                                                     "Will cause a crash if previous filters "
                                                     "are attached to filtergraph.");
      removeSendStream(sessionID, session, localSSRC);
    }

    // actually create the mediastream
    session.outgoingStreams[localSSRC] = new SendStream;
    session.outgoingStreams[localSSRC]->ms = stream;
    session.outgoingStreams[localSSRC]->runZRTP = runZRTP;
    session.outgoingStreams[localSSRC]->sender = nullptr;


    if (localSSRC != 0)
    {
      stream->configure_ctx(RCC_SSRC, localSSRC);
    }

    Logger::getLogger()->printDebug(DEBUG_NORMAL,this, "Created mediastream for sending",
                                     {"SessionID","LocalSSRC","OutgoingCount"},
                                     {QString::number(sessionID), QString::number(localSSRC), QString::number(session.outgoingStreams.size())});
  }

  return true;
}


void Delivery::removeSendStream(uint32_t sessionID, DeliverySession& session,
                                uint32_t localSSRC)
{
  Logger::getLogger()->printNormal(this, "Removing mediastream");

  // If a sender filter exists, stop it and release it before destroying
  // the underlying uvgRTP media_stream. The filter may access the
  // media_stream on background threads, so destroying the stream while
  // the filter is still running can cause crashes.
  if (session.outgoingStreams.find(localSSRC) != session.outgoingStreams.end() &&
      session.outgoingStreams[localSSRC] != nullptr &&
      session.outgoingStreams[localSSRC]->sender != nullptr)
  {
    auto sender = session.outgoingStreams[localSSRC]->sender;
    // stop processing and drain buffers
    sender->stop();
    sender->emptyBuffer();

    // wait for filter to actually stop
    while (sender->isRunning())
    {
      QCoreApplication::processEvents();
      QThread::msleep(1);
    }

    // release our reference so the filter can be destroyed
    session.outgoingStreams[localSSRC]->sender = nullptr;
    sender = nullptr;
  }

  // Now it is safe to destroy the uvgRTP stream and the wrapper
  if (session.outgoingStreams.find(localSSRC) != session.outgoingStreams.end() &&
      session.outgoingStreams[localSSRC] != nullptr)
  {
    session.session->destroy_stream(session.outgoingStreams[localSSRC]->ms);
    delete session.outgoingStreams[localSSRC];
    session.outgoingStreams[localSSRC] = nullptr;
    session.outgoingStreams.erase(localSSRC);
  }
}


void Delivery::removeRecvStream(uint32_t sessionID, DeliverySession& session,
                                uint32_t remoteSSRC)
{
  Logger::getLogger()->printNormal(this, "Removing mediastream");

  // If a receiver filter exists, stop it and release it before destroying
  // the underlying uvgRTP media_stream. The receiver installs hooks into
  // the stream and may be called from uvgRTP threads; releasing the hook
  // after stream destruction would be unsafe.
  if (session.incomingStreams.find(remoteSSRC) != session.incomingStreams.end() &&
      session.incomingStreams[remoteSSRC] != nullptr &&
      session.incomingStreams[remoteSSRC]->receiver != nullptr)
  {
    auto receiver = session.incomingStreams[remoteSSRC]->receiver;
    receiver->stop();
    receiver->emptyBuffer();

    while (receiver->isRunning())
    {
      QCoreApplication::processEvents();
      QThread::msleep(1);
    }

    session.incomingStreams[remoteSSRC]->receiver = nullptr;
    receiver = nullptr;
  }

  if (session.incomingStreams.find(remoteSSRC) != session.incomingStreams.end() &&
      session.incomingStreams[remoteSSRC] != nullptr)
  {
    session.session->destroy_stream(session.incomingStreams[remoteSSRC]->ms);
    delete session.incomingStreams[remoteSSRC];
    session.incomingStreams[remoteSSRC] = nullptr;
    session.incomingStreams.erase(remoteSSRC);
  }
}


void Delivery::removePeer(uint32_t sessionID)
{
  if (peers_.find(sessionID) != peers_.end())
  {
    for (auto& session : peers_[sessionID]->sessions)
    {
      std::vector<uint32_t> localSSRCs;
      std::vector<uint32_t> remoteSSRCs;

      // take all keys so we wont get iterator errors
      for (auto& stream : session.outgoingStreams)
      {
        if (stream.second != nullptr)
        {
          localSSRCs.push_back(stream.first);
        }
      }

      for (auto& stream : session.incomingStreams)
      {
        if (stream.second != nullptr)
        {
          remoteSSRCs.push_back(stream.first);
        }
      }

      // delete all streams
      for (auto& ssrc : localSSRCs)
      {
        removeSendStream(sessionID, session, ssrc);
      }

      for (auto& ssrc : remoteSSRCs)
      {
        removeRecvStream(sessionID, session, ssrc);
      }

      // delete session
      if (rtp_ctx_ && session.session)
      {
        rtp_ctx_->destroy_session(session.session);
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
    // Use the collected id values
    removePeer(ids[i]);
  }

  for (auto& relay : relays_)
  {
    relay.second->stop();
    while (relay.second->isRunning())
    {
      QCoreApplication::processEvents();
    }
  }

  relays_.clear();
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
