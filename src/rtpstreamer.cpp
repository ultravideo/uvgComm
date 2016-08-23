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

RTPStreamer::RTPStreamer():
  senders_(),
  receivers_(),
  nextID_(1),
  iniated_(),
  peer_(),
  ttl_(255),
  sessionAddress_(),
  audioPort_(15555),
  videoPort_(18888),
  stopRTP_(0),
  env_(NULL)
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

  for(auto it : senders_)
  {
    it.second->videoSource = NULL;

    it.second->videoSink->stopPlaying();
    RTPSink::close(it.second->videoSink);

    RTCPInstance::close(it.second->rtcp);

    delete it.second->rtpGroupsock;
    delete it.second->rtcpGroupsock;

    delete it.second->rtpPort;
    delete it.second->rtcpPort;

    delete it.second;
  }
  for(auto it : receivers_)
  {
    it.second->videoSink->stopPlaying();
    it.second->videoSink = NULL; // deleted in filter graph
    FramedSource::close(it.second->videoSource);
    it.second->videoSource = 0;

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

PeerID RTPStreamer::addPeer(in_addr peerAddress, bool video, bool audio)
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
    addH265VideoSend(peerID, peerAddress);
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


void RTPStreamer::addH265VideoSend(PeerID peer, in_addr peerAddress)
{
  if (senders_.find(peer) != senders_.end())
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
  sender->videoSink = H265VideoRTPSink::createNew(*env_, sender->rtpGroupsock, 96);

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
                                   sender->videoSink,
                                   NULL,
                                   False);

  sender->videoSource = new FramedSourceFilter(*env_, HEVCVIDEO);

  if(!sender->videoSource || !sender->videoSink)
  {
    qCritical() << "Failed to setup sending RTP stream";
    return;
  }

  if(!sender->videoSink->startPlaying(*(sender->videoSource), NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }


  senders_[peer] = sender;

}

void RTPStreamer::addH265VideoReceive(PeerID peer, in_addr peerAddress)
{
  if (receivers_.find(peer) != receivers_.end())
  {
    qWarning() << "Peer already exists, not adding to receivers list";
    return;
  }
  Receiver *receiver = new Receiver;


  qDebug() << "Iniating H265 receive video RTP/RTCP streams";
  receiver->rtpPort = new Port(videoPort_);
  //recvRtcpPort_ = new Port(portNum_ + 1);


  receiver->rtpGroupsock = new Groupsock(*env_, sessionAddress_, peerAddress, *receiver->rtpPort);

  //recvRtcpGroupsock_ = new Groupsock(*env_, destinationAddress_, *recvRtcpPort_, ttl_);

  // todo: negotiate payload number
  receiver->videoSource = H265VideoRTPSource::createNew(*env_, receiver->rtpGroupsock, 96);
  //recvVideoSource_ = H265VideoStreamFramer::createNew(*env_, recvRtpGroupsock_);

  receiver->videoSink = new RTPSinkFilter(*env_);

  if(!receiver->videoSource || !receiver->videoSink)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    delete receiver->rtpGroupsock;
    delete receiver->rtpPort;
    delete receiver;
    return;
  }

  if(!receiver->videoSink->startPlaying(*receiver->videoSource,NULL,NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  receivers_[peer] = receiver;
}



