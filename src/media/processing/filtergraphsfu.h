#pragma once

#include "filtergraph.h"

class FilterGraphSFU : public FilterGraph
{
public:
  FilterGraphSFU();

  virtual void uninit();

  virtual void sendVideoto(uint32_t sessionID,
                           std::shared_ptr<Filter> sender,
                           uint32_t localSSRC,
                           uint32_t remoteSSRC,
                           const QString& remoteCNAME,
                           bool isP2P);

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                                VideoInterface *view,
                                uint32_t remoteSSRC);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> sender,
                           uint32_t localSSRC);
  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                                uint32_t remoteSSRC);

protected:

  virtual void lastPeerRemoved();

};
