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

RTPStreamer::RTPStreamer():
  peers_(),
  isIniated_(false),
  isRunning_(false),
  iniated_(),
  destroyed_(),
  ttl_(255),
  sessionAddress_(),
  stopRTP_(0),
  env_(NULL),
  scheduler_(NULL),
  stats_(NULL),
  CNAME_()
{
  // use unicast
  QString ip_str = "0.0.0.0";
  QHostAddress address;
  address.setAddress(ip_str);
  sessionAddress_.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  gethostname((char*)CNAME_, maxCNAMElen_);
  CNAME_[maxCNAMElen_] = '\0'; // just in case
}

 // QThread run function
void RTPStreamer::run()
{
  if(!isIniated_)
  {
    init(stats_);
  }
  qDebug() << "Live555 starting eventloop. TID:" << (uint64_t)currentThreadId();
  stopRTP_ = 0;

  setPriority(QThread::HighPriority);

  isRunning_ = true;
  // returns when stopRTP_ is set to 1
  env_->taskScheduler().doEventLoop(&stopRTP_);
  isRunning_ = false;
  qDebug() << "RTP streamer eventloop stopped";
}

void RTPStreamer::stop()
{
  qDebug() << "Stopping RTP Streamer";
  if(stopRTP_ == 0)
  {
    qDebug() << "RTP Streamer was running as expected";
    stopRTP_ = 1;
  }
}

void RTPStreamer::init(StatisticsInterface *stats)
{
  stats_ = stats;

  iniated_.lock();
  qDebug() << "Iniating live555";
  QString sName(reinterpret_cast<char*>(CNAME_));
  qDebug() << "Our hostname:" << sName;

  scheduler_ = BasicTaskScheduler::createNew();
  if(scheduler_)
    env_ = BasicUsageEnvironment::createNew(*scheduler_);

  OutPacketBuffer::maxSize = 65536;
  isIniated_ = true;
  iniated_.unlock();
}


void RTPStreamer::uninit()
{
  Q_ASSERT(stopRTP_);
  qDebug() << "Uniniating RTP streamer";

  if(isRunning_)
  {
    stop();
  }

  while(isRunning_)
    qSleep(1);

  for(unsigned int i = 0; i < peers_.size(); ++i)
  {
    if(peers_.at(i) != 0)
    {
      removePeer(i);
    }
  }

  if(!env_->reclaim())
  {
    qWarning() << "Warning: Unsuccessful reclaim of usage environment";
  }
  else
  {
    delete scheduler_;
    qDebug() << "Successful reclaim of usage environment";
  }

  isIniated_ = false;

  qDebug() << "RTP streamer uninit completed";
}

PeerID RTPStreamer::addPeer(in_addr ip)
{
  // not being destroyed
  if(destroyed_.tryLock(0))
  {
    iniated_.lock(); // not being iniated

    qDebug() << "Adding peer to following IP: "
             << (uint8_t)((ip.s_addr) & 0xff) << "."
             << (uint8_t)((ip.s_addr >> 8) & 0xff) << "."
             << (uint8_t)((ip.s_addr >> 16) & 0xff) << "."
             << (uint8_t)((ip.s_addr >> 24) & 0xff);

    Peer* peer = new Peer;

    peer->id = (PeerID)peers_.size();
    peer->ip = ip;
    peer->videoSender = 0;
    peer->videoReceiver = 0;
    peer->audioSender = 0;
    peer->audioReceiver = 0;

    peers_.push_back(peer);

    iniated_.unlock();
    destroyed_.unlock();

    qDebug() << "RTP streamer: Peer #" << peer->id << "added";

    return peer->id;
  }
  qWarning() <<  "Trying to add peer while RTP was being destroyed.";

  return -1;
}

void RTPStreamer::removePeer(PeerID id)
{
  qDebug() << "Removing peer" << id << "from RTPStreamer";
  if(peers_.at(id) != 0)
  {
    stop();
    while(isRunning_)
      qSleep(1);

    if(peers_.at(id)->audioSender)
      destroySender(peers_.at(id)->audioSender);
    if(peers_.at(id)->videoSender)
      destroySender(peers_.at(id)->videoSender);
    if(peers_.at(id)->audioReceiver)
      destroyReceiver(peers_.at(id)->audioReceiver);
    if(peers_.at(id)->videoReceiver)
      destroyReceiver(peers_.at(id)->videoReceiver);

    delete peers_.at(id);
    peers_.at(id) = 0;
    start();
  }
  else if(peers_.at(id) == 0)
  {
    qWarning() << "WARNING: Tried to destroy already freed peer:" << id
               << peers_.at(id);
  }
}

void RTPStreamer::destroySender(Sender* sender)
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
    if(sender->sourcefilter)
    {
      // the shared_ptr was initialized not to delete the pointer
      // I don't like live555
      Medium::close(sender->sourcefilter.get());
    }

    destroyConnection(sender->connection);
    delete sender;
  }
  else
    qWarning() << "Warning: Tried to delete sender a second time";
}
void RTPStreamer::destroyReceiver(Receiver* recv)
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
      Medium::close(recv->sink.get());
    }
    if(recv->framedSource)
    {
      Medium::close(recv->framedSource);
      recv->framedSource = NULL;
    }

    destroyConnection(recv->connection);

    delete recv;
  }
  else
    qWarning() << "Warning: Tried to delete receiver a second time";
}

std::shared_ptr<Filter> RTPStreamer::addSendVideo(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->videoSender)
    destroySender(peers_.at(peer)->videoSender);

  peers_.at(peer)->videoSender = addSender(peers_.at(peer)->ip, port, HEVCVIDEO);
  return peers_.at(peer)->videoSender->sourcefilter;
}

std::shared_ptr<Filter> RTPStreamer::addSendAudio(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->audioSender)
    destroySender(peers_.at(peer)->audioSender);

  peers_.at(peer)->audioSender = addSender(peers_.at(peer)->ip, port, OPUSAUDIO);
  return peers_.at(peer)->audioSender->sourcefilter;
}

std::shared_ptr<Filter> RTPStreamer::addReceiveVideo(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->videoReceiver)
    destroyReceiver(peers_.at(peer)->videoReceiver);

  peers_.at(peer)->videoReceiver = addReceiver(peers_.at(peer)->ip, port, HEVCVIDEO);
  return peers_.at(peer)->videoReceiver->sink;
}

std::shared_ptr<Filter> RTPStreamer::addReceiveAudio(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->audioReceiver)
    destroyReceiver(peers_.at(peer)->audioReceiver);

  peers_.at(peer)->audioReceiver = addReceiver(peers_.at(peer)->ip, port, OPUSAUDIO);
  return peers_.at(peer)->audioReceiver->sink;
}

void RTPStreamer::removeSendVideo(PeerID peer)
{
  if(peers_.at(peer)->videoSender)
  {
    destroySender(peers_.at(peer)->videoSender);
    peers_.at(peer)->videoSender = 0;
  }
  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}
void RTPStreamer::removeSendAudio(PeerID peer)
{
  if(peers_.at(peer)->audioSender)
  {
    destroySender(peers_.at(peer)->audioSender);
    peers_.at(peer)->audioSender = 0;
  }

  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}

void RTPStreamer::removeReceiveVideo(PeerID peer)
{
  if(peers_.at(peer)->videoReceiver)
  {
    destroyReceiver(peers_.at(peer)->videoReceiver);
    peers_.at(peer)->videoReceiver = 0;
  }
  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}
void RTPStreamer::removeReceiveAudio(PeerID peer)
{
  if(peers_.at(peer)->audioReceiver)
  {
    destroyReceiver(peers_.at(peer)->audioReceiver);
    peers_.at(peer)->audioReceiver = 0;
  }
  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}

RTPStreamer::Sender* RTPStreamer::addSender(in_addr ip, uint16_t port, DataType type)
{
  qDebug() << "Iniating send RTP/RTCP stream to port:" << port << "With type:" << type;
  Sender* sender = new Sender;
  createConnection(sender->connection, ip, port, false);

  // todo: negotiate payload number
  switch(type)
  {
  case HEVCVIDEO :
    sender->sink = H265VideoRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 96);
    break;
  case RAWAUDIO :
    sender->sink = SimpleRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 0,
                                            48000, "audio", "PCM", 2, False);
    break;
  case OPUSAUDIO :
    sender->sink = SimpleRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 97,
                                            48000, "audio", "OPUS", 2, False);
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }

  QString  ip_str = QString::number((uint8_t)((ip.s_addr) & 0xff)) + "."
                 + QString::number((uint8_t)((ip.s_addr >> 8) & 0xff)) + "."
                 + QString::number((uint8_t)((ip.s_addr >> 16) & 0xff)) + "."
                 + QString::number((uint8_t)((ip.s_addr >> 24) & 0xff));

  // shared_ptr which does not release
  sender->sourcefilter
      = std::shared_ptr<FramedSourceFilter>(new FramedSourceFilter(ip_str + "_", stats_, *env_, type),
                                            [](FramedSourceFilter*){});
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  // This starts RTCP running automatically
  sender->rtcp  = RTCPInstance::createNew(*env_,
                                   sender->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   sender->sink,
                                   NULL,
                                   False);

  if(!sender->sourcefilter || !sender->sink)
  {
    qCritical() << "Failed to setup sending RTP stream";
    return NULL;
  }

  if(!sender->sink->startPlaying(*(sender->sourcefilter.get()), NULL, NULL))
  {
    qCritical() << "Critical: failed to start videosink: " << env_->getResultMsg();
  }
  return sender;
}
// TODO why name peerADDress
RTPStreamer::Receiver* RTPStreamer::addReceiver(in_addr peerAddress, uint16_t port, DataType type)
{
  qDebug() << "Iniating receive RTP/RTCP stream from port:" << port;
  Receiver *receiver = new Receiver;
  createConnection(receiver->connection, peerAddress, port, true);

  unsigned int estimatedSessionBandwidth = 10000; // in kbps; for RTCP b/w share
  // todo: negotiate payload number
  switch(type)
  {
  case HEVCVIDEO :
    receiver->framedSource = H265VideoRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 96);
    break;
  case RAWAUDIO :
    receiver->framedSource = SimpleRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 0,
                                                        48000, "audio/PCM", 0, True);
    break;
  case OPUSAUDIO :
    receiver->framedSource = SimpleRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 97,
                                                        48000, "audio/OPUS", 0, True);
    estimatedSessionBandwidth = 200; // in kbps; for RTCP b/w share
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }
  QString  ip_str = QString::number((uint8_t)((peerAddress.s_addr) & 0xff)) + "."
                 + QString::number((uint8_t)((peerAddress.s_addr >> 8) & 0xff)) + "."
                 + QString::number((uint8_t)((peerAddress.s_addr >> 16) & 0xff)) + "."
                 + QString::number((uint8_t)((peerAddress.s_addr >> 24) & 0xff));

  // shared_ptr which does not release
  receiver->sink
      = std::shared_ptr<RTPSinkFilter>(new RTPSinkFilter(ip_str + "_", stats_, *env_, type),
                                       [](RTPSinkFilter*){});

  // This starts RTCP running automatically
  receiver->rtcp  = RTCPInstance::createNew(*env_,
                                   receiver->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   NULL,
                                   receiver->framedSource, // this is the client
                                   False);

  if(!receiver->framedSource || !receiver->sink)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    RTCPInstance::close(receiver->rtcp);
    destroyConnection(receiver->connection);
    delete receiver;
    return NULL;
  }

  if(!receiver->sink->startPlaying(*receiver->framedSource,NULL,NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  return receiver;
}

void RTPStreamer::createConnection(Connection& connection, struct in_addr ip,
                                   uint16_t portNum, bool reservePorts)
{
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

void RTPStreamer::destroyConnection(Connection& connection)
{
  if(connection.rtpGroupsock)
  {
    connection.rtpGroupsock->removeAllDestinations();
    delete connection.rtpGroupsock;
    connection.rtpGroupsock = 0;
  }
  if(connection.rtcpGroupsock)
  {
    connection.rtcpGroupsock->removeAllDestinations();
    delete connection.rtcpGroupsock;
    connection.rtcpGroupsock = 0;
  }

  if(connection.rtpPort)
  {
    delete connection.rtpPort;
    connection.rtpPort = 0;
  }
  if(connection.rtcpPort)
  {
    delete connection.rtcpPort;
    connection.rtcpPort = 0;
  }
}
