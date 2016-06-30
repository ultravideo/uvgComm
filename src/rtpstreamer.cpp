#include "rtpstreamer.h"

#include "framedsourcefilter.h"

RTPStreamer::RTPStreamer(UsageEnvironment* env): env_(env){}

void RTPStreamer::run()
{
  env_->taskScheduler().doEventLoop();
}
