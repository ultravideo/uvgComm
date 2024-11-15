#pragma once

#include "filtergraph.h"

#include <vector>

class FiltergraphSFU : public FilterGraph
{
public:
  FiltergraphSFU();

  virtual void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                           const MediaID &id);

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                VideoInterface *view,
                                const MediaID &id);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                           const MediaID &id);
  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                                const MediaID &id);

  void addParticipantToSFU(uint32_t sessionID,
                           std::pair<std::shared_ptr<Filter>, MediaID>& videoReceiver,
                           std::pair<std::shared_ptr<Filter>, MediaID>& audioReceiver,
                           std::vector<std::pair<std::shared_ptr<Filter>, MediaID>>& videoSenders,
                           std::vector<std::pair<std::shared_ptr<Filter>, MediaID>> audioSenders,
                           VideoInterface* view);

};
