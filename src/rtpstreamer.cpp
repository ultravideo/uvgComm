#include "rtpstreamer.h"

#include "framedsourcefilter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>

#include <QHostInfo>
#include <QDebug>

#include <iostream>

RTPStreamer::RTPStreamer():
  portNum_(18888),
  env_(NULL),
  rtpPort_(NULL),
  rtcpPort_(NULL),
  ttl_(255),
  videoSink_(NULL),
  videoSource_(NULL),
  destinationAddress_()
{
  live555_.lock();
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
  qDebug() << "Iniating RTP streamer";

  initLiveMedia();
  initH265Video();
  initOpusAudio();

  live555_.unlock();
  qDebug() << "Iniating RTP streamer finished, do eventloop";


  env_->taskScheduler().doEventLoop();

}

void RTPStreamer::initLiveMedia()
{
  qDebug() << "Iniating live555";

  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  if(scheduler)
    env_ = BasicUsageEnvironment::createNew(*scheduler);
}

void RTPStreamer::initH265Video()
{
  qDebug() << "Iniating H265 video RTP/RTCP streams";
  rtpPort_ = new Port(portNum_);
  rtcpPort_ = new Port(portNum_ + 1);

  rtpGroupsock_ = new Groupsock(*env_, destinationAddress_, *rtpPort_, ttl_);
  rtcpGroupsock_ = new Groupsock(*env_, destinationAddress_, *rtcpPort_, ttl_);

  // Create a 'H265 Video RTP' sink from the RTP 'groupsock':
  OutPacketBuffer::maxSize = 1000000;
  videoSink_ = H265VideoRTPSink::createNew(*env_, rtpGroupsock_, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share

  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case

  QString sName(reinterpret_cast<char*>(CNAME));
  qDebug() << "Our hostname:" << sName;

  // This starts RTCP running automatically
  rtcp_  = RTCPInstance::createNew(*env_,
                                   rtcpGroupsock_,
                                   estimatedSessionBandwidth,
                                   CNAME,
                                   videoSink_,
                                   NULL,
                                   False);

  videoSource_ = new FramedSourceFilter(*env_, HEVCVIDEO);

  if(!videoSource_ || !videoSink_)
  {
    qCritical() << "Failed to setup RTP stream";
  }

  if(!videoSink_->startPlaying(*videoSource_, NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }
}

void RTPStreamer::initOpusAudio()
{
  qWarning() << "Opus RTP not implemented yet";
}



