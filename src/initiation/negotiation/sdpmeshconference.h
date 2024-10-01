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

  void updateTemplateMedia(uint32_t sessionID, QList<MediaInfo>& medias);

  // returns whether the SSRC was found and set
  bool setCorrespondingSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                            int index, QList<MediaInfo>& medias);

  // generates the SSRC and sets it to media
  void generateSSRC(uint32_t sessionID, uint32_t mediaSessionID, MediaInfo& media);

  bool findCname(MediaInfo& media, QString& cname) const;
  bool findSSRC(MediaInfo& media, uint32_t& ssrc) const;
  bool findMID(MediaInfo& media, int& mid) const;

  int nextMID(uint32_t sessionID);
  void removeMID(MediaInfo& media);

  void updateMediaState(MediaInfo& currentState, const MediaInfo &newState);
  bool verifyMediaInfoMatch(const MediaInfo& currentState, const MediaInfo& newState) const;

  void handleSSRCUpdate(QList<QList<SDPAttribute>>& currentAttributes,
                        const QList<QList<SDPAttribute>>& newAttributes);

  MeshType type_;

  /* These are the templates used to form new connections between other participants that us.
   *
   * In mesh with multiplexing, there is no need to update the candidate ports and therefore
   * these templates can be used as is for each participant without further modifications.
   *
   * If multiplexing is not used in mesh, then the ports must be increased for each participant
   * that is added to the call.
   */

  std::map<uint32_t, QList<MediaInfo>> singleSDPTemplates_;
  std::map<uint32_t, QList<MediaInfo>> preparedMessages_;

  struct GeneratedSSRC
  {
    uint32_t ssrc;
    QString  cname;
    uint32_t sessionID;
  };

  // first key is sessionID who this SSRC was sent to
  // the second key is mid
  std::unordered_map<uint32_t, std::unordered_map<int, GeneratedSSRC>> generatedSSRCs_;

  std::unordered_map<uint32_t, QString> cnames_;
  std::unordered_map<uint32_t, int> nextMID_;
};
