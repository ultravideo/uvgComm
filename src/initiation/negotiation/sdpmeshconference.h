#pragma once

#include "sdptypes.h"

#include <cstdint>
#include <unordered_map>

enum MeshType
{
  MESH_NO_CONFERENCE,
  MESH_WITH_RTP_MULTIPLEXING,
  MESH_WITHOUT_RTP_MULTIPLEXING
};

class SDPMeshConference
{
public:
  SDPMeshConference();

  void uninit();

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

  // needed for keeping track of connections if connection multiplexing is not supported
  typedef std::map<uint32_t, std::shared_ptr<SDPMessageInfo>> MeshConnections;
  std::map<uint32_t, MeshConnections> remoteConnections_;

  /* These are the templates used to form new connections between other participants that us.
   *
   * In mesh with multiplexing, there is no need to update the candidate ports and therefore
   * these templates can be used as is for each participant without further modifications.
   *
   * If multiplexing is not used in mesh, then the ports must be increased for each participant
   * that is added to the call.
   */
  std::map<uint32_t, std::shared_ptr<SDPMessageInfo>> singleSDPTemplates_;

  // first key is sessionID, second key is remote sessionID. Value is generated SSRCs in order
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, QString>>>> generatedSSRCs_;
};
