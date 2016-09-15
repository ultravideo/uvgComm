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
  audioPort_(15555),
  videoPort_(18888),
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

PeerID RTPStreamer::addPeer(in_addr peerAddress, uint16_t framerate, bool video, bool audio)
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

  if(video)
  {
    addH265VideoSend(peerID, peerAddress, framerate);
    addH265VideoReceive(peerID, peerAddress);
  }

  peer_.unlock();
  iniated_.unlock();

  qDebug() << "RTP streamer: Peer" << peerID << "added";

  return peerID;
}

void RTPStreamer::removePeer(PeerID id)
{

}

void RTPStreamer::addH265VideoSend(PeerID peer, in_addr peerAddress, uint16_t framerate)
{
  if (videoSenders_.find(peer) != videoSenders_.end())
  {
    qWarning() << "Peer already exists, not adding to senders list";
    return;
  }

  qDebug() << "Iniating H265 send video RTP/RTCP streams";

  Sender* sender = new Sender;

  createConnection(sender->connection, peerAddress, videoPort_, false);

  // todo: negotiate payload number
  sender->sink = H265VideoRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 96);



  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share


  // This starts RTCP running automatically
  sender->rtcp  = RTCPInstance::createNew(*env_,
                                   sender->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   sender->sink,
                                   NULL,
                                   False);

  sender->framedSource = new FramedSourceFilter(stats_, *env_, HEVCVIDEO);



  if(!sender->framedSource || !sender->sink)
  {
    qCritical() << "Failed to setup sending RTP stream";
    return;
  }

  if(!sender->sink->startPlaying(*(sender->framedSource), NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }


  videoSenders_[peer] = sender;

}

void RTPStreamer::addH265VideoReceive(PeerID peer, in_addr peerAddress)
{
  if (videoReceivers_.find(peer) != videoReceivers_.end())
  {
    qWarning() << "Peer already exists, not adding to receivers list";
    return;
  }
  Receiver *receiver = new Receiver;

  createConnection(receiver->connection, peerAddress, videoPort_, true);

  qDebug() << "Iniating H265 receive video RTP/RTCP streams";

  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share

  // todo: negotiate payload number
  receiver->framedSource = H265VideoRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 96);

  // This starts RTCP running automatically
  receiver->rtcp  = RTCPInstance::createNew(*env_,
                                   receiver->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   NULL,
                                   receiver->framedSource, // this is the client
                                   False);

  receiver->sink = new RTPSinkFilter(stats_, *env_);

  if(!receiver->framedSource || !receiver->sink)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    destroyConnection(receiver->connection);
    delete receiver;
    return;
  }

  if(!receiver->sink->startPlaying(*receiver->framedSource,NULL,NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  videoReceivers_[peer] = receiver;
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

