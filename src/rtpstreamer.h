#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include "framedsourcefilter.h"

class FramedSourceFilter;

class RTPStreamer : public QThread
{

  Q_OBJECT

public:
  RTPStreamer();

  void run();

  FramedSourceFilter* getFilter()
  {
    live555_.lock();
    live555_.unlock();
    return videoSource_;
  }

private:

  QMutex live555_;

  UsageEnvironment* env_;

  RTPSink* videoSink_;
  FramedSourceFilter* videoSource_;

  struct in_addr destinationAddress_;

};

#endif // RTPSTREAMER_H
