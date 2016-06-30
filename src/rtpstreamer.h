#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include "framedsourcefilter.h"



class RTPStreamer : public QThread
{

  Q_OBJECT

public:
  RTPStreamer(UsageEnvironment* env);

  void run();

private:

  UsageEnvironment* env_;

};

#endif // RTPSTREAMER_H
