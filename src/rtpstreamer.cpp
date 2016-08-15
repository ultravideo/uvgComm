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
  iniated_(false),
  portNum_(18888),
  ttl_(255),
  sessionAddress_(),
  stopRTP_(0),
  env_(NULL)
{
  // use unicast
  QString ip_str = "0.0.0.0";
  QHostAddress address;
  address.setAddress(ip_str);
  sessionAdress_.S_un.S_addr = qToBigEndian(address.toIPv4Address());
}

void RTPStreamer::setDestination(in_addr address, uint16_t port)
{
  destinationAddress_ = address;
  portNum_ = port;

  qDebug() << "Destination IP address: "
           << (uint8_t)((destinationAddress_.s_addr) & 0xff) << "."
           << (uint8_t)((destinationAddress_.s_addr >> 8) & 0xff) << "."
           << (uint8_t)((destinationAddress_.s_addr >> 16) & 0xff) << "."
           << (uint8_t)((destinationAddress_.s_addr >> 24) & 0xff);
}

void RTPStreamer::run()
{

  if(!iniated_)
  {
    qDebug() << "Iniating RTP streamer";
    initLiveMedia();
    iniated_ = true;
    qDebug() << "Iniating RTP streamer finished";
  }

  qDebug() << "RTP streamer starting eventloop";

  stopRTP_ = 0;
  env_->taskScheduler().doEventLoop(&stopRTP_);

  qDebug() << "RTP streamer eventloop stopped";

  uninit();

}

void RTPStreamer::stop()
{
  stopRTP_ = 1;
}

PeerID RTPStreamer::addPeer(in_addr peerAddress, bool video, bool audio)
{

  PeerID peerID = nextID_;
  ++nextID_;

  if(video)
  {
    addH265VideoSend(peerID, peerAddress);
    addH265VideoReceive(peerID, peerAddress);
  }

  return peerID;
}

void RTPStreamer::removePeer(PeerID id)
{

}


void RTPStreamer::uninit()
{
  Q_ASSERT(stopRTP_);
  if(iniated_)
  {
    qDebug() << "Uniniating RTP streamer";
    iniated_ = false;


    for(auto it : senders_)
    {
      it->second()->sendVideoSource_ = NULL;

      it->second()->videoSink->stopPlaying();
      RTPSink::close(it->second->videoSink_);

      RTCPInstance::close(it->second()->rtcp);

      delete it->second->rtpGroupsock;
      delete it->second->rtcpGroupsock;

      delete it->second->rtpPort;
      delete it->second->rtcpPort;

      delete it->second;
    }

    recvVideoSink_->stopPlaying();
    recvVideoSink_ = NULL; // deleted in filter graph
    FramedSource::close(recvVideoSource_);
    recvVideoSource_ = 0;

    delete recvRtpGroupsock_;
    recvRtpGroupsock_ = 0;

    delete recvRtpPort_;
    recvRtpPort_ = 0;




    if(!env_->reclaim())
      qWarning() << "Unsuccesful reclaim of usage environment";

  }
  else
  {
    qWarning() << "Double uninit for RTP streamer";
  }
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

void RTPStreamer::addH265VideoSend(PeerID peer, in_addr peerAddress)
{
  qDebug() << "Iniating H265 send video RTP/RTCP streams";

  Sender* sender;

  sender->rtpPort = new Port(0); // 0 because it reserves the port for some reason
  sender->rtcpPort = new Port(portNum_ + 1);

  sender->rtpGroupsock = new Groupsock(*env_, sessionAddress_, peerAddress, *(sender->rtpPort));
  sender->rtpGroupsock->changeDestinationParameters(peerAddress, portNum_, ttl_);

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

void RTPStreamer::addH265VideoReceive()
{
  qDebug() << "Iniating H265 receive video RTP/RTCP streams";
  recvRtpPort_ = new Port(portNum_);
  //recvRtcpPort_ = new Port(portNum_ + 1);



  recvRtpGroupsock_ = new Groupsock(*env_, sessionAddress_, destinationAddress_, *recvRtpPort_);

  //recvRtcpGroupsock_ = new Groupsock(*env_, destinationAddress_, *recvRtcpPort_, ttl_);

  // todo: negotiate payload number
  recvVideoSource_ = H265VideoRTPSource::createNew(*env_, recvRtpGroupsock_, 96);
  //recvVideoSource_ = H265VideoStreamFramer::createNew(*env_, recvRtpGroupsock_);

  recvVideoSink_ = new RTPSinkFilter(*env_);

  if(!recvVideoSource_ || !recvVideoSink_)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    return;
  }

  if(!recvVideoSink_->startPlaying(*recvVideoSource_,NULL,NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }
}



