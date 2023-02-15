#pragma once

#include "sdptypes.h"

#include <cstdint>

class SDPMeshConference
{
public:
  SDPMeshConference();

  void setConferenceMode(bool state);

  void addP2PSDP(uint32_t sessionID, SDPMessageInfo& sdp);
  void removeP2PSDP(uint32_t sessionID);

  std::shared_ptr<SDPMessageInfo> getMeshSDP(uint32_t sessionID,
                                             std::shared_ptr<SDPMessageInfo> sdp);

private:

  MediaInfo copyMedia(MediaInfo& media);

  std::shared_ptr<ICEInfo> updateICECandidate(std::shared_ptr<ICEInfo> candidate, int components, int foundation);

  bool conferenceMode_;

  std::map<uint32_t, std::shared_ptr<SDPMessageInfo>> singleSDPs_;

};
