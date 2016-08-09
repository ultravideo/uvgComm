#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>


class FramedSourceFilter;
class RTPSinkFilter;

class RTPStreamer : public QThread
{
  Q_OBJECT

public:
  RTPStreamer();

  void setDestination(in_addr address, uint16_t port);

  void run();

  void stop();

  FramedSourceFilter* getSourceFilter()
  {
    if(iniated_)
      return sendVideoSource_;
    return NULL;
  }

  RTPSinkFilter* getSinkFilter()
  {
    if(iniated_)
      return recvVideoSink_;
    return NULL;
  }


private:

  void initLiveMedia();
  void initH265VideoSend();
  void initH265VideoReceive();
  void initOpusAudio();

  void uninit();

  bool iniated_;

  uint16_t portNum_;

  UsageEnvironment* env_;

  Port* sendRtpPort_;
  Port* sendRtcpPort_;
  Port* recvRtpPort_;
  //Port* recvRtcpPort_;
  uint8_t ttl_;

  Groupsock* sendRtpGroupsock_;
  Groupsock* sendRtcpGroupsock_;
  Groupsock* recvRtpGroupsock_;
  //Groupsock* recvRtcpGroupsock_;
  RTCPInstance* rtcp_;

  RTPSink* sendVideoSink_;
  FramedSourceFilter* sendVideoSource_;

  FramedSource* recvVideoSource_;
  RTPSinkFilter* recvVideoSink_;

  struct in_addr destinationAddress_;

  char stopRTP_;

};

#endif // RTPSTREAMER_H
