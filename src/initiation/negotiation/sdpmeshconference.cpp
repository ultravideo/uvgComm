#include "sdpmeshconference.h"

SDPMeshConference::SDPMeshConference()
{}


void SDPMeshConference::setConferenceMode(bool state)
{
  conferenceMode_ = state;
}


void SDPMeshConference::addP2PSDP(uint32_t sessionID, SDPMessageInfo& sdp)
{
  std::shared_ptr<SDPMessageInfo> referenceSDP =
      std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *referenceSDP = sdp;
  singleSDPs_[sessionID] = referenceSDP;
}


void SDPMeshConference::removeP2PSDP(uint32_t sessionID)
{
  singleSDPs_.erase(sessionID);
}


std::shared_ptr<SDPMessageInfo> SDPMeshConference::getMeshSDP(uint32_t sessionID,
                                                              std::shared_ptr<SDPMessageInfo> sdp)
{
  if (false)
  {
    // TODO: Add all required medias (non-sessionID) and add suitable ICE candidates for those medias

    int foundation = 1;
    for (auto& session : singleSDPs_)
    {
      // do not add their own media to the message
      if (session.first != sessionID)
      {
        int components = 0;
        for (auto& media : session.second->media)
        {
          if (media.proto == "RTP/AVP")
          {
            components += 2; // RTP and RTCP
          }
          else
          {
            components += 1;
          }

          for (int i = 0; i < media.candidates.size(); ++i)
          {
            media.candidates[i] = updateICECandidate(media.candidates[i],
                                                     components, foundation);
          }

          ++foundation;

          sdp->media.push_back(copyMedia(media));
        }
      }
    }
  }

  return sdp;
}


MediaInfo SDPMeshConference::copyMedia(MediaInfo& media)
{
  return media;
}


std::shared_ptr<ICEInfo> SDPMeshConference::updateICECandidate(std::shared_ptr<ICEInfo> candidate,
                                                               int components, int foundation)
{
  std::shared_ptr<ICEInfo> newCandidate = std::shared_ptr<ICEInfo>(new ICEInfo);
  *newCandidate = *candidate;
  if (newCandidate->port != 0)
  {
    newCandidate->port += components;
  }

  if (newCandidate->rel_port != 0)
  {
    newCandidate->rel_port += components;
  }

  newCandidate->foundation = foundation;

  return candidate;
}
