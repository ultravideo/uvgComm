#pragma once

#include "sdptypes.h"

#include <cstdint>

enum MeshType
{
  MESH_NO_CONFERENCE,
  MESH_JOINT_RTP_SESSION,
  MESH_INDEPENDENT_RTP_SESSION
};

class SDPMeshConference
{
public:
  SDPMeshConference();

  void setConferenceMode(MeshType type);

  void addRemoteSDP(uint32_t sessionID, SDPMessageInfo& sdp);
  void removeRemoteSDP(uint32_t sessionID);

  std::shared_ptr<SDPMessageInfo> getMeshSDP(uint32_t sessionID,
                                             std::shared_ptr<SDPMessageInfo> sdp);

private:

  std::shared_ptr<SDPMessageInfo> getTemplateSDP(SDPMessageInfo& sdp);
  void updateGeneratedSDPs(SDPMessageInfo& sdp);

  MediaInfo copyMedia(MediaInfo& media);

  std::shared_ptr<ICEInfo> updateICECandidate(std::shared_ptr<ICEInfo> candidate, int components);

  MeshType type_;

  /* Collection of SDPs that can be used to form the next SDP,
   * the ports are update whenever a new participant joins.
   * Key: sessionID */
  std::map<uint32_t, std::shared_ptr<SDPMessageInfo>> singleSDPTemplates_;


  /* Collection of SDPs ready to be sent. Each new participant both adds
   * their conference SDP here and also adds their media to existing ones.
   * Key: sessionID */
  std::map<uint32_t, std::shared_ptr<SDPMessageInfo>> generatedSDPs_;
};
