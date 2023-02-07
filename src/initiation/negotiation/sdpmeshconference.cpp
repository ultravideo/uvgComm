#include "sdpmeshconference.h"

SDPMeshConference::SDPMeshConference()
{}


void SDPMeshConference::setConferenceMode(bool state)
{
  conferenceMode_ = state;
}

void SDPMeshConference::addP2PSDP(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp)
{
  if (!conferenceMode_)
  {
    singleSDPs_[sessionID] = sdp;
  }
}

void SDPMeshConference::getMeshSDP(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp)
{
  if (conferenceMode_)
  {
    // TODO: Add all required medias (non-sessionID) and add suitable ICE candidates for those medias
  }
}
