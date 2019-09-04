#include "rtpstreamer.h"

#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "common.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <iostream>

Live555RTP::Live555RTP():
  peers_(),
  isIniated_(false),
  isRunning_(false),
  iniated_(),
  destroyed_(),
  ttl_(255),
  sessionAddress_(),
  stopRTP_(0),
  env_(nullptr),
  scheduler_(nullptr),
  stats_(nullptr),
  CNAME_()
{
  // use unicast
  QString ip_str = "0.0.0.0";
  QHostAddress address;
  address.setAddress(ip_str);
#ifdef _WIN32
  sessionAddress_.S_un.S_addr = qToBigEndian(address.toIPv4Address());
#else
  sessionAddress_.s_addr = qToBigEndian(address.toIPv4Address());
#endif

  gethostname((char*)CNAME_, maxCNAMElen_);
  CNAME_[maxCNAMElen_] = '\0'; // just in case

  triggerMutex_ = new QMutex;
}

Live555RTP::~Live555RTP()
{
  delete triggerMutex_;
}

 // QThread run function
void Live555RTP::run()
{
  if(!isIniated_)
  {
    init(stats_);
  }
  qDebug() << "Iniating: Live555 starting eventloop. TID:" << (uint64_t)currentThreadId();
  stopRTP_ = 0;

  setPriority(QThread::HighestPriority);

  isRunning_ = true;
  // returns when stopRTP_ is set to 1
  env_->taskScheduler().doEventLoop(&stopRTP_);
  isRunning_ = false;
  qDebug() << "Closing," << metaObject()->className() << ": RTP streamer eventloop stopped. TID:" << (uint64_t)currentThreadId();
}

void Live555RTP::stop()
{
  qDebug() << "Closing," << metaObject()->className()
           << "Stopping RTP Streamer";

  if(stopRTP_ == 0)
  {
    qDebug() << "Closing," << metaObject()->className() << "RTP Streamer was running as expected";
    stopRTP_ = 1;
  }
}

void Live555RTP::init(StatisticsInterface *stats)
{
  stats_ = stats;

  iniated_.lock();
  qDebug() << "Iniating: live555";
  QString sName(reinterpret_cast<char*>(CNAME_));
  qDebug() << "Iniating: Our hostname:" << sName;

  scheduler_ = BasicTaskScheduler::createNew();
  if(scheduler_)
    env_ = BasicUsageEnvironment::createNew(*scheduler_);

  //OutPacketBuffer::maxSize = 65536;
  OutPacketBuffer::maxSize = 65536*512;
  isIniated_ = true;
  iniated_.unlock();
}

void Live555RTP::uninit()
{
  Q_ASSERT(stopRTP_);
  qDebug() << "Closing," << metaObject()->className() << "Uniniating RTP streamer";

  removeAllPeers();

  if(isRunning_)
  {
    stop();
  }

  while(isRunning_)
    qSleep(1);

  if(!env_->reclaim())
  {
    qWarning() << "Warning: Unsuccessful reclaim of usage environment";
  }
  else
  {
    delete scheduler_;
    qDebug() << "Closing," << metaObject()->className() << ": Successful reclaim of usage environment";
  }

  isIniated_ = false;

  qDebug() << "Closing,"<< metaObject()->className() << ": RTP streamer uninit completed";
}

void Live555RTP::removeAllPeers()
{
  for(int i = 0; i < peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      removePeer(i + 1); // The sessionID is + 1
    }
  }
}

bool Live555RTP::addPeer(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  // not being destroyed
  if(destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    qDebug() << "Adding peer";

    Peer* peer = new Peer;
    if(peers_.size() >= sessionID && peers_.at(sessionID - 1) == nullptr)
    {
      peers_[sessionID - 1] = peer;
    }
    else
    {
      while(peers_.size() < sessionID - 1)
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

void Live555RTP::removePeer(uint32_t sessionID)
{
  qDebug() << "Removing peer" << sessionID << "from RTPStreamer";
  if(peers_.at(sessionID - 1) != nullptr)
  {
    stop();
    while(isRunning_)
    {
      qSleep(1);
    }

    for (auto sender : peers_.at(sessionID - 1)->senders)
    {
      destroySender(sender);
      sender = nullptr;
    }
    peers_.at(sessionID - 1)->senders.clear();

    for (auto receiver : peers_.at(sessionID - 1)->receivers)
    {
      destroyReceiver(receiver);
      receiver = nullptr;
    }
    peers_.at(sessionID - 1)->receivers.clear();

    delete peers_.at(sessionID - 1);
    peers_[sessionID - 1] = nullptr;

    // TODO: this may crash, because eventloop may start processing tasks on handletimout.
    qSleep(1);
    start();
    while(!isRunning_)
    {
      qSleep(1);
    }
  }
  else if(peers_.at(sessionID - 1) == nullptr)
  {
    qWarning() << "WARNING: Tried to destroy already freed peer:" << sessionID
               << peers_.at(sessionID - 1);
  }
}

void Live555RTP::destroySender(Sender* sender)
{
  Q_ASSERT(sender);
  if(sender)
  {
    qDebug() << "Destroying sender:" << sender;

    // order of destruction is important!
    if(sender->sink)
    {
      sender->sink->stopPlaying();
    }
    if(sender->rtcp)
    {
      RTCPInstance::close(sender->rtcp);
    }
    if(sender->sink)
    {
      Medium::close(sender->sink);
    }
    if(sender->framerSource)
    {
      Medium::close(sender->framerSource);
    }
    if(sender->sourcefilter)
    {
      // the shared_ptr was initialized not to delete the pointer
      // I don't like live555
      sender->sourcefilter->stop();
      sender->sourcefilter->emptyBuffer();

      while(sender->sourcefilter->isRunning())
      {
        qSleep(1);
      }

      Medium::close(sender->sourcefilter.get());
    }

    destroyConnection(sender->connection);
    delete sender;
  }
  else
    qWarning() << "Warning: Tried to delete sender a second time";
}
void Live555RTP::destroyReceiver(Receiver* recv)
{
  Q_ASSERT(recv);
  if(recv)
  {
    qDebug() << "Destroying receiver:" << recv;

    // order of destruction is important!
    if(recv->framedSource)
    {
      recv->framedSource->stopGettingFrames();
      recv->framedSource->handleClosure();
    }
    if(recv->sink)
    {
      recv->sink->stopPlaying();
      recv->sink->emptyBuffer();
      recv->sink->stop();
    }

    if(recv->rtcp)
    {
      RTCPInstance::close(recv->rtcp);
    }
    if(recv->sink)
    {
      recv->sink->uninit();
      // the shared_ptr was initialized not to delete the pointer
      // I don't like live555
      recv->sink->stop();
      recv->sink->emptyBuffer();

      while(recv->sink->isRunning())
      {
        qSleep(1);
      }

      Medium::close(recv->sink.get());
    }
    if(recv->framedSource)
    {
      Medium::close(recv->framedSource);
      recv->framedSource = nullptr;
    }

    destroyConnection(recv->connection);

    delete recv;
  }
  else
    qWarning() << "Warning: Tried to delete receiver a second time";
}

bool Live555RTP::checkSessionID(uint32_t sessionID)
{
  return peers_.size() >= sessionID
      && peers_.at(sessionID - 1) != nullptr;
}

std::shared_ptr<Filter> Live555RTP::addSendStream(uint32_t sessionID, QHostAddress ip, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(peers_.size() >= sessionID);

  Peer* peer = peers_.at(sessionID - 1);

  struct in_addr ip_addr;

#ifdef _WIN32
  ip_addr.S_un.S_addr = qToBigEndian(ip.toIPv4Address());
#else
  ip_addr.s_addr = qToBigEndian(ip.toIPv4Address());
#endif

  Live555RTP::Sender *sender = addSender(ip_addr, port, typeFromString(codec), rtpNum);

  if (sender == nullptr)
  {
    return nullptr;
  }

  peer->senders.push_back(sender);
  return sender->sourcefilter;
}

std::shared_ptr<Filter> Live555RTP::addReceiveStream(uint32_t sessionID, QHostAddress ip, uint16_t port, QString codec, uint8_t rtpNum)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(peers_.size() >= sessionID);

  Peer* peer = peers_.at(sessionID - 1);

  struct in_addr ip_addr;

#ifdef _WIN32
  ip_addr.S_un.S_addr = qToBigEndian(ip.toIPv4Address());
#else
  ip_addr.s_addr = qToBigEndian(ip.toIPv4Address());
#endif

  Live555RTP::Receiver *receiver = addReceiver(ip_addr, port, typeFromString(codec), rtpNum);

  if (receiver == nullptr)
  {
    return nullptr;
  }

  peer->receivers.push_back(receiver);
  return receiver->sink;
}


Live555RTP::Sender* Live555RTP::addSender(in_addr ip, uint16_t port, DataType type, uint8_t rtpNum)
{
  qDebug() << "Iniating send RTP/RTCP stream to port:" << port << "With type:" << type;
  Sender* sender = new Sender;
  createConnection(sender->connection, ip, port, false);

  // todo: negotiate payload number
  QString mediaName = QString::number(port);
  switch(type)
  {
  case HEVCVIDEO :
    sender->sink = H265VideoRTPSink::createNew(*env_, sender->connection.rtpGroupsock, rtpNum);
    mediaName += "_HEVC";
    break;
  case RAWAUDIO :
    sender->sink = SimpleRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 0,
                                            48000, "audio", "PCM", 2, False);
    mediaName += "_RAW_AUDIO";
    break;
  case OPUSAUDIO :
    sender->sink = SimpleRTPSink::createNew(*env_, sender->connection.rtpGroupsock, rtpNum,
                                            48000, "audio", "OPUS", 2, False);
    mediaName += "_OPUS";
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    mediaName += "_UNKNOWN";
    break;
  }

  QString  ip_str = QString::number(static_cast<uint8_t>((ip.s_addr) & 0xff)) + "."
                 + QString::number(static_cast<uint8_t>((ip.s_addr >> 8) & 0xff)) + "."
                 + QString::number(static_cast<uint8_t>((ip.s_addr >> 16) & 0xff)) + "."
                 + QString::number(static_cast<uint8_t>((ip.s_addr >> 24) & 0xff));

  // shared_ptr which does not release
  sender->sourcefilter
      = std::shared_ptr<FramedSourceFilter>(new FramedSourceFilter(ip_str + "_", stats_, *env_, type,
                                                                   mediaName, triggerMutex_, type != HEVCVIDEO),
                                            [](FramedSourceFilter*){});

  type = NONE;

  if(type == HEVCVIDEO)
  {
    sender->framerSource = H265VideoStreamDiscreteFramer::createNew(*env_, sender->sourcefilter.get());
    //sender->framerSource = H265VideoStreamFramer::createNew(*env_, sender->sourcefilter.get());
  }
  else
  {
    sender->framerSource = nullptr;
  }

  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  // This starts RTCP running automatically
  sender->rtcp  = RTCPInstance::createNew(*env_,
                                   sender->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   sender->sink,
                                   nullptr,
                                   False);

  if(!sender->sourcefilter || !sender->sink)
  {
    qCritical() << "Failed to setup sending RTP stream";
    return nullptr;
  }

  // TODO better code needed


  if(type == HEVCVIDEO)
  {
    if(!sender->sink->startPlaying(*(sender->framerSource), nullptr, nullptr))
    {
      qCritical() << "Critical: failed to start videosink: " << env_->getResultMsg();
    }
  }
  else
  {
    if(!sender->sink->startPlaying(*(sender->sourcefilter.get()), nullptr, nullptr))
    {
      qCritical() << "Critical: failed to start videosink: " << env_->getResultMsg();
    }
  }
  return sender;
}
// TODO why name peerADDress
Live555RTP::Receiver* Live555RTP::addReceiver(in_addr peerAddress, uint16_t port, DataType type, uint8_t rtpNum)
{
  qDebug() << "Iniating receive RTP/RTCP stream from port:" << port << "with type:" << type;
  Receiver *receiver = new Receiver;
  createConnection(receiver->connection, peerAddress, port, true);

  unsigned int estimatedSessionBandwidth = 10000; // in kbps; for RTCP b/w share
  // todo: negotiate payload number
  QString mediaName = QString::number(port);
  switch(type)
  {
  case HEVCVIDEO :
    receiver->framedSource = H265VideoRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, rtpNum);
    mediaName += "_HEVC";
    break;
  case RAWAUDIO :
    receiver->framedSource = SimpleRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, rtpNum,
                                                        48000, "audio/PCM", 0, True);
    mediaName += "_RAW_AUDIO";
    break;
  case OPUSAUDIO :
    receiver->framedSource = SimpleRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, rtpNum,
                                                        48000, "audio/OPUS", 0, True);
    estimatedSessionBandwidth = 200; // in kbps; for RTCP b/w share
    mediaName += "_OPUS";
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }
  QString  ip_str = QString::number(static_cast<uint8_t>((peerAddress.s_addr) & 0xff)) + "."
                 + QString::number(static_cast<uint8_t>((peerAddress.s_addr >> 8) & 0xff)) + "."
                 + QString::number(static_cast<uint8_t>((peerAddress.s_addr >> 16) & 0xff)) + "."
                 + QString::number(static_cast<uint8_t>((peerAddress.s_addr >> 24) & 0xff));

  // shared_ptr which does not release
  receiver->sink
      = std::shared_ptr<RTPSinkFilter>(new RTPSinkFilter(ip_str + "_", stats_, *env_, type, mediaName),
                                       [](RTPSinkFilter*){});

  // This starts RTCP running automatically
  receiver->rtcp  = RTCPInstance::createNew(*env_,
                                   receiver->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   nullptr,
                                   receiver->framedSource, // this is the client
                                   False);

  if(!receiver->framedSource || !receiver->sink)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    RTCPInstance::close(receiver->rtcp);
    destroyConnection(receiver->connection);
    delete receiver;
    return nullptr;
  }

  if(!receiver->sink->startPlaying(*receiver->framedSource, nullptr, nullptr))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  return receiver;
}

void Live555RTP::createConnection(Connection& connection, struct in_addr ip,
                                   uint16_t portNum, bool reservePorts)
{
  // TODO: Sending should not reserve ports.
  // This could mean the same connection should be used for receiving

  if(!reservePorts)
  {
    connection.rtpPort = new Port(0);
    connection.rtcpPort = new Port(0);
  }
  else
  {
    connection.rtpPort = new Port(portNum);
    connection.rtcpPort = new Port(portNum + 1);
  }

  connection.rtpGroupsock = new Groupsock(*env_, sessionAddress_, ip, *connection.rtpPort);
  connection.rtcpGroupsock = new Groupsock(*env_, ip, *(connection.rtcpPort), ttl_);

  if(!reservePorts)
  {
    connection.rtpGroupsock->changeDestinationParameters(ip, portNum, ttl_);
    connection.rtcpGroupsock->changeDestinationParameters(ip, portNum + 1, ttl_);
  }
}

void Live555RTP::destroyConnection(Connection& connection)
{
  if(connection.rtpGroupsock)
  {
    connection.rtpGroupsock->removeAllDestinations();
    delete connection.rtpGroupsock;
    connection.rtpGroupsock = nullptr;
  }
  if(connection.rtcpGroupsock)
  {
    connection.rtcpGroupsock->removeAllDestinations();
    delete connection.rtcpGroupsock;
    connection.rtcpGroupsock = nullptr;
  }

  if(connection.rtpPort)
  {
    delete connection.rtpPort;
    connection.rtpPort = nullptr;
  }
  if(connection.rtcpPort)
  {
    delete connection.rtcpPort;
    connection.rtcpPort = nullptr;
  }
}

DataType Live555RTP::typeFromString(QString type)
{
  std::map<QString, DataType> xmap
      = {{"pcm", RAWAUDIO}, {"opus", OPUSAUDIO},{"h265", HEVCVIDEO}};

  if(xmap.find(type) == xmap.end())
  {
    qDebug() << "ERROR: Tried to use non-defined codec type in RTPSreamer.";
    return NONE;
  }

  return xmap[type];
}