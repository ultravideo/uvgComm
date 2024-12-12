#pragma once

#include "filtergraph.h"

class FilterGraphSFU : public FilterGraph
{
public:
  FilterGraphSFU();

  virtual void uninit();

  virtual void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                           uint32_t localSSRC);

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                VideoInterface *view,
                                uint32_t remoteSSRC);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                           uint32_t localSSRC);
  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                                uint32_t remoteSSRC);

protected:

  virtual void lastPeerRemoved();

};
