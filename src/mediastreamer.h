#pragma once

#include "framedsourcefilter.h"


class Mediastreamer
{
public:
  Mediastreamer();

  int init(uint32_t rtpPortNum, uint32_t rtcpPortNum,
            uint8_t ttl);

  Filter* getVideoSender()
  {
    Q_ASSERT(videoSource_);
    return videoSource_;
  }

  void run();

  void stop();

private:

  UsageEnvironment* env_;
  RTPSink* videoSink_;

  FramedSourceFilter* videoSource_;

  RTPStreamer* scheduler_;

  struct in_addr destinationAddress_;
};
