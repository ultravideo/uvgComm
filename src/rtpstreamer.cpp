#include "rtpstreamer.h"

#include "framedsourcefilter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <iostream>

RTPStreamer::RTPStreamer(StatisticsInterface* stats):
  audioSenders_(),
  videoSenders_(),
  audioReceivers_(),
  videoReceivers_(),
  nextID_(1),
  iniated_(),
  peer_(),
  ttl_(255),
  sessionAddress_(),
  stopRTP_(0),
  env_(NULL),
  stats_(stats),
  CNAME_()
{
  // use unicast
  QString ip_str = "0.0.0.0";
  QHostAddress address;
  address.setAddress(ip_str);
  sessionAddress_.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  gethostname((char*)CNAME_, maxCNAMElen_);
  CNAME_[maxCNAMElen_] = '\0'; // just in case

  iniated_.lock();
}

void RTPStreamer::run()
{
  qDebug() << "Iniating RTP streamer";
  initLiveMedia();
  iniated_.unlock();
  qDebug() << "Iniating RTP streamer finished. "
           << "RTP streamer starting eventloop";

  stopRTP_ = 0;
  env_->taskScheduler().doEventLoop(&stopRTP_);

  qDebug() << "RTP streamer eventloop stopped";

  uninit();

}

void RTPStreamer::stop()
{
  stopRTP_ = 1;
}

void RTPStreamer::uninit()
{
  Q_ASSERT(stopRTP_);
  qDebug() << "Uniniating RTP streamer";

  for(auto it : videoSenders_)
  {
    RTCPInstance::close(it.second->rtcp);
    it.second->framedSource = NULL;
    it.second->sink->stopPlaying();
    RTPSink::close(it.second->sink);
    destroyConnection(it.second->connection);

    delete it.second;
  }
  for(auto it : videoReceivers_)
  {
    RTCPInstance::close(it.second->rtcp);
    it.second->sink->stopPlaying();
    it.second->sink = NULL; // deleted in filter graph
    FramedSource::close(it.second->framedSource);
    it.second->framedSource = 0;
    destroyConnection(it.second->connection);
  }

  if(!env_->reclaim())
    qWarning() << "Unsuccesful reclaim of usage environment";

  qDebug() << "RTP streamer uninit succesful";
}

void RTPStreamer::initLiveMedia()
{
  qDebug() << "Iniating live555";
  QString sName(reinterpret_cast<char*>(CNAME_));
  qDebug() << "Our hostname:" << sName;

  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  if(scheduler)
    env_ = BasicUsageEnvironment::createNew(*scheduler);

  OutPacketBuffer::maxSize = 65536;
}

PeerID RTPStreamer::addPeer(in_addr peerAddress,
                            uint16_t videoPort, uint16_t audioPort)
{
  iniated_.lock();
  peer_.lock();
  qDebug() << "Adding peer to following IP: "
           << (uint8_t)((peerAddress.s_addr) & 0xff) << "."
           << (uint8_t)((peerAddress.s_addr >> 8) & 0xff) << "."
           << (uint8_t)((peerAddress.s_addr >> 16) & 0xff) << "."
           << (uint8_t)((peerAddress.s_addr >> 24) & 0xff);

  PeerID peerID = nextID_;
  ++nextID_;

  if(videoPort)
  {
    videoSenders_[peerID] = addSendRTP(peerAddress, videoPort, HEVCVIDEO);
    videoReceivers_[peerID] = addReceiveRTP(peerAddress, videoPort, HEVCVIDEO);
  }

  peer_.unlock();
  iniated_.unlock();

  qDebug() << "RTP streamer: Peer" << peerID << "added";

  return peerID;
}

void RTPStreamer::removePeer(PeerID id)
{}

RTPStreamer::Sender* RTPStreamer::addSendRTP(in_addr ip, uint16_t port, DataType type)
{
  qDebug() << "Iniating send RTP/RTCP stream";

  Sender* sender = new Sender;
  createConnection(sender->connection, ip, port, false);

  // todo: negotiate payload number
  switch(type)
  {
  case HEVCVIDEO :
    sender->sink = H265VideoRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 96);
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }
  sender->framedSource = new FramedSourceFilter(stats_, *env_, type);
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  // This starts RTCP running automatically
  sender->rtcp  = RTCPInstance::createNew(*env_,
                                   sender->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   sender->sink,
                                   NULL,
                                   False);

  if(!sender->framedSource || !sender->sink)
  {
    qCritical() << "Failed to setup sending RTP stream";
    return NULL;
  }

  if(!sender->sink->startPlaying(*(sender->framedSource), NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }
  return sender;
}

RTPStreamer::Receiver* RTPStreamer::addReceiveRTP(in_addr peerAddress, uint16_t port, DataType type)
{
  qDebug() << "Iniating receive RTP/RTCP stream";
  Receiver *receiver = new Receiver;
  createConnection(receiver->connection, peerAddress, port, true);

  // todo: negotiate payload number
  switch(type)
  {
  case HEVCVIDEO :
    receiver->framedSource = H265VideoRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 96);
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }

  receiver->sink = new RTPSinkFilter(stats_, *env_);

  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
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
    delete connection.rtpGroupsock;
    connection.rtpGroupsock = 0;
  }
  if(connection.rtcpGroupsock)
  {
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

// use these to ask for filters that are connected to filter graph
FramedSourceFilter* RTPStreamer::getSourceFilter(PeerID peer, DataType type)
{
  peer_.lock();
  // TODO: Check availability
//   Q_ASSERT(videoSenders_.find(peer) != videoSenders_.end());
  peer_.unlock();
  if(type == RAWAUDIO)
    return audioSenders_[peer]->framedSource;
  else if(type == HEVCVIDEO)
    return videoSenders_[peer]->framedSource;

  return NULL;
}

RTPSinkFilter* RTPStreamer::getSinkFilter(PeerID peer, DataType type)
{
  peer_.lock();
//    Q_ASSERT(receivers_.find(peer) != receivers_.end());
  peer_.unlock();
  if(type == RAWAUDIO)
    return audioReceivers_[peer]->sink;
  else if(type == HEVCVIDEO)
    return videoReceivers_[peer]->sink;

  return NULL;
}

