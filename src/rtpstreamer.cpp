#include "rtpstreamer.h"

#include <BasicUsageEnvironment.hh>

RTPStreamer::RTPStreamer():
  env_(NULL),
  videoSource_(NULL),
  videoSink_(NULL)
{
  name_ = "RTPSF";

}

void RTPStreamer::init(uint32_t rtpPortNum, uint32_t rtcpPortNum, uint8_t ttl)
{
  scheduler_ = BasicTaskScheduler::createNew();
  env_ = BasicUsageEnvironment::createNew(*scheduler_);
  destinationAddress_.s_addr = chooseRandomIPv4SSMAddress(*env_);

  const Port rtpPort(rtpPortNum);
  const Port rtcpPort(rtcpPortNum);

  Groupsock rtpGroupsock(*env_, destinationAddress_, rtpPort, ttl);
  rtpGroupsock.multicastSendOnly(); // we're a SSM source

  Groupsock rtcpGroupsock(*env_, destinationAddress_, rtcpPort, ttl);
  rtcpGroupsock.multicastSendOnly(); // we're a SSM source

  // Create a 'H265 Video RTP' sink from the RTP 'groupsock':
  OutPacketBuffer::maxSize = 100000;
  videoSink_ = H265VideoRTPSink::createNew(*env_, &rtpGroupsock, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
//  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case
  RTCPInstance* rtcp  = RTCPInstance::createNew(*env_,
                                                &rtcpGroupsock,
                                                estimatedSessionBandwidth,
                                                CNAME,
                                                videoSink_,
                                                NULL /* we're a server */,
                                                True /* we're a SSM source */);
  // Note: This starts RTCP running automatically

}

void RTPStreamer::process()
{

}
