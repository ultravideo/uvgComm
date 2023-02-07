#include "sdpmeshconference.h"

SDPMeshConference::SDPMeshConference()
{}


void SDPMeshConference::setConferenceMode(bool state)
{
  conferenceMode_ = state;
}

void SDPMeshConference::addP2PSDP(uint32_t sessionID, SDPMessageInfo& sdp)
{
  if (!conferenceMode_)
  {
    std::shared_ptr<SDPMessageInfo> referenceSDP =
        std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *referenceSDP = sdp;
    singleSDPs_[sessionID] = referenceSDP;
  }
}


void SDPMeshConference::removeP2PSDP(uint32_t sessionID)
{
  singleSDPs_.erase(sessionID);
}


std::shared_ptr<SDPMessageInfo> SDPMeshConference::getMeshSDP(uint32_t sessionID,
                                                              std::shared_ptr<SDPMessageInfo> sdp)
{
  if (conferenceMode_)
  {
    // TODO: Add all required medias (non-sessionID) and add suitable ICE candidates for those medias
  }

  return sdp;
}
