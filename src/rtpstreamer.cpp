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
  iniated_(false),
  portNum_(18888),
  env_(NULL),
  sendRtpPort_(NULL),
  sendRtcpPort_(NULL),
  recvRtpPort_(NULL),
//  inputRtcpPort_(NULL),
  ttl_(255),
  sendVideoSink_(NULL),
  sendVideoSource_(NULL),
  recvVideoSource_(NULL),
  destinationAddress_()
{}

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
    initH265VideoSend();
    initOpusAudio();
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

void RTPStreamer::uninit()
{
  Q_ASSERT(stopRTP_);
  if(iniated_)
  {
    qDebug() << "Uniniating RTP streamer";
    iniated_ = false;
    sendVideoSource_ = NULL;
    sendVideoSink_->stopPlaying();

    RTPSink::close(sendVideoSink_);
    RTCPInstance::close(rtcp_);

    delete sendRtpGroupsock_;
    sendRtpGroupsock_ = 0;
    delete sendRtcpGroupsock_;
    sendRtcpGroupsock_ = 0;

    delete sendRtpPort_;
    sendRtpPort_ = 0;
    delete sendRtcpPort_;
    sendRtcpPort_ = 0;

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
}

void RTPStreamer::initH265VideoSend()
{
  qDebug() << "Iniating H265 video RTP/RTCP streams";
  sendRtpPort_ = new Port(portNum_);
  sendRtcpPort_ = new Port(portNum_ + 1);

  sendRtpGroupsock_ = new Groupsock(*env_, destinationAddress_, *sendRtpPort_, ttl_);
  sendRtcpGroupsock_ = new Groupsock(*env_, destinationAddress_, *sendRtcpPort_, ttl_);

  // Create a 'H265 Video RTP' sink from the RTP 'groupsock':
  OutPacketBuffer::maxSize = 65536;
  // todo: negotiate payload number
  sendVideoSink_ = H265VideoRTPSink::createNew(*env_, sendRtpGroupsock_, 96);

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
                                   sendRtcpGroupsock_,
                                   estimatedSessionBandwidth,
                                   CNAME,
                                   sendVideoSink_,
                                   NULL,
                                   False);

  sendVideoSource_ = new FramedSourceFilter(*env_, HEVCVIDEO);

  if(!sendVideoSource_ || !sendVideoSink_)
  {
    qCritical() << "Failed to setup RTP stream";
    return;
  }

  if(!sendVideoSink_->startPlaying(*sendVideoSource_, NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }
}

void RTPStreamer::initH265VideoReceive()
{
  qDebug() << "Iniating H265 video RTP/RTCP streams";
  recvRtpPort_ = new Port(portNum_);
  //recvRtcpPort_ = new Port(portNum_ + 1);

  recvRtpGroupsock_ = new Groupsock(*env_, destinationAddress_, *recvRtpPort_, ttl_);
  //recvRtcpGroupsock_ = new Groupsock(*env_, destinationAddress_, *recvRtcpPort_, ttl_);

  // todo: negotiate payload number
  recvVideoSource_ = H265VideoRTPSource::createNew(*env_, recvRtpGroupsock_, 96);
  //recvVideoSource_ = H265VideoStreamFramer::createNew(*env_, recvRtpGroupsock_);
}


void RTPStreamer::initOpusAudio()
{
  qWarning() << "Audio RTP not implemented yet";
}



