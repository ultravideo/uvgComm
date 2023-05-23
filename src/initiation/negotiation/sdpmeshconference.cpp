#include "sdpmeshconference.h"

#include "logger.h"
#include "common.h"

const int LOCAL_MEDIAS = 2;

SDPMeshConference::SDPMeshConference():
  type_(MESH_NO_CONFERENCE)
{}


void SDPMeshConference::setConferenceMode(MeshType type)
{
  type_ = type;
}


void SDPMeshConference::uninit()
{
  singleSDPTemplates_.clear();
}


void SDPMeshConference::addRemoteSDP(uint32_t sessionID, SDPMessageInfo& sdp)
{
  std::shared_ptr<SDPMessageInfo> referenceSDP =
      std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *referenceSDP = sdp;
  singleSDPTemplates_[sessionID] = referenceSDP;

  while (singleSDPTemplates_[sessionID]->media.size() > 2)
  {
    // remove media meant for us
    singleSDPTemplates_[sessionID]->media.pop_front();
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
    case MESH_WITH_RTP_MULTIPLEXING:
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
    case MESH_WITHOUT_RTP_MULTIPLEXING:
    {
      if (meshSDP->media.size() > 2)
      {
        for (unsigned int i = meshSDP->media.size(); i > 2; --i)
        {
          meshSDP->media.pop_back();
        }
      }

      for (auto& session : singleSDPTemplates_)
      {
        int components = 4;

        // do not add their own media to the message
        if (session.first != sessionID)
        {
          for (auto& media : session.second->media)
          {
            // if this is the outgoing extrapolated message, we increase candidate ports
            if (singleSDPTemplates_.find(sessionID) == singleSDPTemplates_.end())
            {
              for (int i = 0; i < media.candidates.size(); ++i)
              {
                media.candidates[i] = updateICECandidate(media.candidates[i],
                                                         components);
              }

              media.receivePort += components;
            }

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
                                                               int components)
{
  Logger::getLogger()->printNormal("SDPMeshConference", "Updating candidate");
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

  return newCandidate;
}
