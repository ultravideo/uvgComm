#pragma once

#include "sdptypes.h"

#include <cstdint>
#include <unordered_map>

enum ConferenceType
{
  SDP_CONF_NONE,
  SDP_CONF_LOCAL_MESH,
  SDP_CONF_MESH,
  SDP_CONF_LOCAL_SFU,
  SDP_CONF_SFU,
  SDP_CONF_LOCAL_MCU,
  SDP_CONF_MCU,
  SDP_CONF_LOCAL_RELAY,
  SDP_CONF_RELAY,
  SDP_CONF_HYBRID
};

/* This class handles the generation of P2P Mesh conference via SDP Messages.
 *
 * addSDPPair() handles the recording of the local and remote SDP media information
 * as well as SSRC relations. The getMeshSDP() function is used to generate each
 * SDP message needed for forming the P2P Mesh conference using information store
 * with addSDPPair() function.
 */


class SDPConference
{
public:
  SDPConference();

  void uninit();

  void setConferenceMode(ConferenceType type);

  void addRemoteSDP(uint32_t sessionID, SDPMessageInfo& sdp);
  void removeSession(uint32_t sessionID);

  std::shared_ptr<SDPMessageInfo> getConferenceSDP(uint32_t sessionID,
                                             std::shared_ptr<SDPMessageInfo> localSDP);

private:

  std::shared_ptr<SDPMessageInfo> getMeshSDP(uint32_t sessionID,
                                       std::shared_ptr<SDPMessageInfo> localSDP);

  std::shared_ptr<SDPMessageInfo> getSFUSDP(uint32_t sessionID,
                                            std::shared_ptr<SDPMessageInfo> localSDP);


  std::shared_ptr<SDPMessageInfo> getHybridSDP(uint32_t sessionID,
                                               std::shared_ptr<SDPMessageInfo> localSDP);

  std::shared_ptr<SDPMessageInfo> getTemplateSDP(SDPMessageInfo& sdp);
  void updateGeneratedSDPs(SDPMessageInfo& sdp);

  MediaInfo copyMedia(MediaInfo& media);

  void updateTemplateMedia(uint32_t sessionID, QList<MediaInfo>& medias);

  // returns whether the SSRC was found and set
  bool setCorrespondingSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                            int index, QList<MediaInfo>& medias);

  // generates the SSRC and sets it to media
  void generateSSRC(uint32_t sessionID, uint32_t mediaSessionID, MediaInfo& media);

  int nextMID(uint32_t sessionID);
  void removeMID(MediaInfo& media);

  void updateMediaState(MediaInfo& currentState, const MediaInfo &newState);
  bool verifyMediaInfoMatch(const MediaInfo& currentState, const MediaInfo& newState) const;

  void handleSSRCUpdate(QList<QList<SDPAttribute>>& currentAttributes,
                        const QList<QList<SDPAttribute>>& newAttributes);

  void distributeMediaToSessions(uint32_t sessionID, SDPMessageInfo &sdp);

  void updateAllMedias(SDPMessageInfo &sdp);

  void distributeSSRCs(uint32_t sessionID, SDPMessageInfo &sdp);

  ConferenceType type_;

  /* These are the templates used to form new connections between other participants that us.
   *
   * In mesh with multiplexing, there is no need to update the candidate ports and therefore
   * these templates can be used as is for each participant without further modifications.
   *
   * If multiplexing is not used in mesh, then the ports must be increased for each participant
   * that is added to the call.
   */

  std::map<uint32_t, QList<MediaInfo>> p2pSingleSDPTemplates_;
  std::map<uint32_t, QList<MediaInfo>> p2pPreparedMessages_;

  std::map<uint32_t, QList<MediaInfo>> sfuSingleSDPTemplates_;
  std::map<uint32_t, QList<MediaInfo>> sfuPreparedMessages_;


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
