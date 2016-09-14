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
  stats_(stats)
{
  // use unicast
  QString ip_str = "0.0.0.0";
  QHostAddress address;
  address.setAddress(ip_str);
  sessionAddress_.S_un.S_addr = qToBigEndian(address.toIPv4Address());

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
    it.second->framedSource = NULL;

    it.second->sink->stopPlaying();
    RTPSink::close(it.second->sink);

    RTCPInstance::close(it.second->rtcp);

    delete it.second->rtpGroupsock;
    delete it.second->rtcpGroupsock;

    delete it.second->rtpPort;
    delete it.second->rtcpPort;

    delete it.second;
  }
  for(auto it : videoReceivers_)
  {
    it.second->sink->stopPlaying();
    it.second->sink = NULL; // deleted in filter graph
    FramedSource::close(it.second->framedSource);
    it.second->framedSource = 0;

    delete it.second->rtpGroupsock;
    delete it.second->rtpPort;
    delete it.second;
  }

  if(!env_->reclaim())
    qWarning() << "Unsuccesful reclaim of usage environment";

  qDebug() << "RTP streamer uninit succesful";
}

void RTPStreamer::initLiveMedia()
{
  qDebug() << "Iniating live555";

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

  sender->rtpPort = new Port(0); // 0 because it reserves the port for some reason
  sender->rtcpPort = new Port(videoPort_ + 1);

  sender->rtpGroupsock = new Groupsock(*env_, sessionAddress_, peerAddress, *(sender->rtpPort));
  sender->rtpGroupsock->changeDestinationParameters(peerAddress, videoPort_, ttl_);

  sender->rtcpGroupsock = new Groupsock(*env_, peerAddress, *(sender->rtcpPort), ttl_);


  // todo: negotiate payload number
  sender->sink = H265VideoRTPSink::createNew(*env_, sender->rtpGroupsock, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share

  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case

  QString sName(reinterpret_cast<char*>(CNAME));
  qDebug() << "Our hostname:" << sName;

  // This starts RTCP running automatically
  sender->rtcp  = RTCPInstance::createNew(*env_,
                                   sender->rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME,
                                   sender->sink,
                                   NULL,
                                   False);

  sender->framedSource = new FramedSourceFilter(stats_, *env_, HEVCVIDEO, framerate);

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

  qDebug() << "Iniating H265 receive video RTP/RTCP streams";
  receiver->rtpPort = new Port(videoPort_);
  receiver->rtcpPort = new Port(videoPort_ + 1);

  receiver->rtpGroupsock = new Groupsock(*env_, sessionAddress_, peerAddress, *receiver->rtpPort);

  receiver->rtcpGroupsock = new Groupsock(*env_, peerAddress, *(receiver->rtcpPort), ttl_);

  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case



  // todo: negotiate payload number
  receiver->framedSource = H265VideoRTPSource::createNew(*env_, receiver->rtpGroupsock, 96);

  // This starts RTCP running automatically
  receiver->rtcp  = RTCPInstance::createNew(*env_,
                                   receiver->rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME,
                                   NULL,
                                   receiver->framedSource, // this is the client
                                   False);

  receiver->sink = new RTPSinkFilter(stats_, *env_);

  if(!receiver->framedSource || !receiver->sink)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    delete receiver->rtpGroupsock;
    delete receiver->rtpPort;
    delete receiver;
    return;
  }

  if(!receiver->sink->startPlaying(*receiver->framedSource,NULL,NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  videoReceivers_[peer] = receiver;
}
