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

  FramedSourceFilter* getFilter()
  {
    live555_.lock();
    live555_.unlock();
    return videoSource_;
  }

private:

  void initLiveMedia();
  void initH265Video();
  void initOpusAudio();

//  in_addr address_;
  uint16_t portNum_;

  QMutex live555_;

  UsageEnvironment* env_;

  Port* rtpPort_;
  Port* rtcpPort_;
  uint8_t ttl_;

  Groupsock* rtpGroupsock_;
  Groupsock* rtcpGroupsock_;

  RTPSink* videoSink_;
  FramedSourceFilter* videoSource_;

  struct in_addr destinationAddress_;

};

#endif // RTPSTREAMER_H
