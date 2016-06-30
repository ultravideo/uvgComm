#include "rtpstreamer.h"

#include "framedsourcefilter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>


#include <QDebug>

RTPStreamer::RTPStreamer():
//  iniated_(false),
  env_(NULL),
  videoSink_(NULL),
  videoSource_(NULL),
  destinationAddress_()
{
  live555_.lock();
}

void RTPStreamer::run()
{
  qDebug() << "Iniating mediastreamer";

  uint32_t rtpPortNum = 18888;
  uint32_t rtcpPortNum = 18889;
  uint8_t ttl = 255;

  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env_ = BasicUsageEnvironment::createNew(*scheduler);
  //destinationAddress_.s_addr = chooseRandomIPv4SSMAddress(*env_);
  destinationAddress_.s_addr = ourIPAddress(*env_);

  qDebug() << (uint8_t)((destinationAddress_.s_addr >> 24) & 0xff) << "."
           << (uint8_t)((destinationAddress_.s_addr >> 16) & 0xff) << "."
           << (uint8_t)((destinationAddress_.s_addr >> 8) & 0xff) << "."
           << (uint8_t)((destinationAddress_.s_addr) & 0xff);

  const Port rtpPort(rtpPortNum);
  const Port rtcpPort(rtcpPortNum);

  Groupsock rtpGroupsock(*env_, destinationAddress_, rtpPort, ttl);
  //rtpGroupsock.multicastSendOnly(); // we're a SSM source

  Groupsock rtcpGroupsock(*env_, destinationAddress_, rtcpPort, ttl);
 // rtcpGroupsock.multicastSendOnly(); // we're a SSM source

  // Create a 'H265 Video RTP' sink from the RTP 'groupsock':
  OutPacketBuffer::maxSize = 1000000;
  videoSink_ = H265VideoRTPSink::createNew(*env_, &rtpGroupsock, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  const unsigned int maxCNAMElen = 100;
  unsigned char CNAME[] = "localhost";
  //strcpy((char*)CNAME, QHostInfo::localHostName().toStdString().c_str());
  //CNAME[maxCNAMElen] = '\0'; // just in case
  RTCPInstance* rtcp  = RTCPInstance::createNew(*env_,
                                                &rtcpGroupsock,
                                                estimatedSessionBandwidth,
                                                CNAME,
                                                videoSink_,
                                                NULL,
                                                False);
  // Note: This starts RTCP running automatically


  videoSource_ = new FramedSourceFilter(*env_, HEVCVIDEO);

  if(!videoSource_ || !videoSink_)
  {
    qCritical() << "Failed to setup RTP stream";
  }

  if(!videoSink_->startPlaying(*videoSource_, NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  live555_.unlock();
  qDebug() << "Iniating finished, do eventloop";

  env_->taskScheduler().doEventLoop();
}
