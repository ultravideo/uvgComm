#include "sdpmeshconference.h"

#include "logger.h"
#include "common.h"

SDPMeshConference::SDPMeshConference():
  type_(MESH_NO_CONFERENCE)
{}


void SDPMeshConference::setConferenceMode(MeshType type)
{
  type_ = type;
}


void SDPMeshConference::addRemoteSDP(uint32_t sessionID, SDPMessageInfo& sdp)
{
  std::shared_ptr<SDPMessageInfo> referenceSDP =
      std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *referenceSDP = sdp;
  singleSDPTemplates_[sessionID] = referenceSDP;

  std::vector<int> toDelete;
  for (unsigned int i = 1; i < singleSDPTemplates_[sessionID]->media.size(); ++i)
  {
    bool alreadyExists = false;

    for (unsigned int j = 0; j < i; ++j)
    {
      if (areMediasEqual(singleSDPTemplates_[sessionID]->media.at(i),
                         singleSDPTemplates_[sessionID]->media.at(j)) &&
          !singleSDPTemplates_[sessionID]->media.at(i).candidates.empty() &&
          singleSDPTemplates_[sessionID]->media.at(i).candidates.size() ==
          singleSDPTemplates_[sessionID]->media.at(j).candidates.size() &&
          sameCandidate(singleSDPTemplates_[sessionID]->media.at(i).candidates.first(),
                        singleSDPTemplates_[sessionID]->media.at(j).candidates.first()))
      {
        alreadyExists = true;
        break;
      }
    }

    if (alreadyExists)
    {
      toDelete.push_back(i);
    }
  }

  for (int i = toDelete.size() - 1; i >= 0; --i)
  {
    singleSDPTemplates_[sessionID]->media.erase(singleSDPTemplates_[sessionID]->media.begin() +
                                                toDelete.at(i));
  }
}


void SDPMeshConference::removeRemoteSDP(uint32_t sessionID)
{
  singleSDPTemplates_.erase(sessionID);
}


std::shared_ptr<SDPMessageInfo> SDPMeshConference::getMeshSDP(uint32_t sessionID,
                                                              std::shared_ptr<SDPMessageInfo> sdp)
{
  std::shared_ptr<SDPMessageInfo> meshSDP = sdp;

  switch(type_)
  {
    case MESH_NO_CONFERENCE:
    {
      break;
    }
    case MESH_JOINT_RTP_SESSION:
    {
      meshSDP = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
      *meshSDP = *sdp;

      for (auto& storedSDP : singleSDPTemplates_)
      {
        if (storedSDP.first != sessionID)
        {
          meshSDP->media += storedSDP.second->media;
        }
      }
      break;
    }
    case MESH_INDEPENDENT_RTP_SESSION:
    {
    // TODO: Add all required medias (non-sessionID) and add suitable ICE candidates for those medias

      int foundation = 1;
      for (auto& session : singleSDPTemplates_)
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
      break;
    }
    default:
    {
      Logger::getLogger()->printUnimplemented("SDPMeshConference", "Unimplemented mesh conferencing mode");
      break;
    }
  }

  return meshSDP;
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
