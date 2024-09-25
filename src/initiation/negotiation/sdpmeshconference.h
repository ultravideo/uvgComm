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

/* This class handles the generation of P2P Mesh conference via SDP Messages.
 *
 * addSDPPair() handles the recording of the local and remote SDP media information
 * as well as SSRC relations. The getMeshSDP() function is used to generate each
 * SDP message needed for forming the P2P Mesh conference using information store
 * with addSDPPair() function.
 */


class SDPMeshConference
{
public:
  SDPMeshConference();

  void uninit();

  void setConferenceMode(MeshType type);

  void addRemoteSDP(uint32_t sessionID, SDPMessageInfo& sdp);
  void removeSession(uint32_t sessionID);

  std::shared_ptr<SDPMessageInfo> getMeshSDP(uint32_t sessionID,
                                             std::shared_ptr<SDPMessageInfo> localSDP);

private:

  std::shared_ptr<SDPMessageInfo> getTemplateSDP(SDPMessageInfo& sdp);
  void updateGeneratedSDPs(SDPMessageInfo& sdp);

  MediaInfo copyMedia(MediaInfo& media);

  // returns whether the SSRC was found and set
  bool setCorrespondingSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                            int index, QList<MediaInfo>& medias);

  // generates the SSRC and sets it to media
  void generateSSRC(uint32_t sessionID, uint32_t mediaSessionID, int index, QList<MediaInfo>& medias, int mid);

  // sets the generated SSRC to media
  void setGeneratedSSRC(uint32_t sessionID, uint32_t mediaSessionID, int index, QList<MediaInfo>& medias);

  void setMid(MediaInfo& media, int mid);

  bool findCname(MediaInfo& media, QString& cname) const;
  bool findSSRC(MediaInfo& media, uint32_t& ssrc) const;
  bool findMID(MediaInfo& media, int& mid) const;

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
  std::map<uint32_t, QList<MediaInfo>> singleSDPTemplates_;

  struct GeneratedSSRC
  {
    uint32_t generatedSSRC;
    QString cname;

    int correspondingMID;
    uint32_t correspondingSSRC;
    QString correspondingCname;
  };

  // first key is sessionID who the generated SSRC will belong to
  // second key is sessionID of the counter pair
  std::unordered_map<uint32_t,
                     std::unordered_map<uint32_t,
                                        std::vector<GeneratedSSRC>>> generatedSSRCs_;

  std::unordered_map<uint32_t, QString> cnames_;
};
