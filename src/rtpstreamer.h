#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include "framedsourcepile.h"



class RTPStreamer : public Filter
{
public:
  RTPStreamer();

  void init(uint32_t rtpPortNum, uint32_t rtcpPortNum, uint8_t ttl);

  virtual bool isInputFilter() const
  {
    return false;
  }

  virtual bool isOutputFilter() const
  {
    return true;
  }

protected:

  void process();


private:

  UsageEnvironment* env_;
  FramedSourcePile* videoSource_;
  RTPSink* videoSink_;
  TaskScheduler* scheduler_;

  struct in_addr destinationAddress_;


  //TaskScheduler* scheduler_;
  //struct in_addr destinationAddress_;


};

#endif // RTPSTREAMER_H
