#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "framedsourcefilter.h"
#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>


class FramedSourceFilter;

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
      return videoSource_;
    return NULL;
  }

private:

  void initLiveMedia();
  void initH265Video();
  void initOpusAudio();

  void uninit();

  bool iniated_;

//  in_addr address_;
  uint16_t portNum_;

  UsageEnvironment* env_;

  Port* rtpPort_;
  Port* rtcpPort_;
  uint8_t ttl_;

  Groupsock* rtpGroupsock_;
  Groupsock* rtcpGroupsock_;

  RTCPInstance* rtcp_;

  RTPSink* videoSink_;
  FramedSourceFilter* videoSource_;

  struct in_addr destinationAddress_;

  char stopRTP_;

};

#endif // RTPSTREAMER_H
