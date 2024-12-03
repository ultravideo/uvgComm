#pragma once

#include "filtergraph.h"

class FilterGraphSFU : public FilterGraph
{
public:
  FilterGraphSFU();

  virtual void uninit();

  virtual void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                           const MediaID &id);

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                VideoInterface *view,
                                const MediaID &id);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                           const MediaID &id);
  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                                const MediaID &id);

protected:

  virtual void lastPeerRemoved();

};
