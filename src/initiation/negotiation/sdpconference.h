#pragma once

#include "sdptypes.h"

#include <cstdint>
#include <unordered_map>

enum ConferenceType
{
  SDP_CONF_NONE,
  SDP_CONF_LOCAL_MESH, // we participate in the mesh
  SDP_CONF_MESH,       // we do not participate in the mesh, just host it
  SDP_CONF_LOCAL_SFU,  // we are the SFU
  SDP_CONF_SFU,        // not implemented
  SDP_CONF_LOCAL_MCU,  // not implemented
  SDP_CONF_MCU,        // not implemented
  SDP_CONF_LOCAL_RELAY, // not implemented
  SDP_CONF_RELAY,       // not implemented
  SDP_CONF_HYBRID       // hybrid between SFU and Mesh, we are the SFU
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

  // records the details from received message so they
  // can be used to generate new SDP messages
  void recordReceivedSDP(uint32_t sessionID, SDPMessageInfo& sdp);
  void removeSession(uint32_t sessionID);

  std::shared_ptr<SDPMessageInfo> generateConferenceMedia(uint32_t sessionID,
                                             std::shared_ptr<SDPMessageInfo> localSDP);

private:

  // get the SDP media information for specific sessionID in each conference type
  void getMeshMedia(uint32_t sessionID,   QList<MediaInfo>& sdpMedia);
  void getSFUMedia(uint32_t sessionID, const QList<MediaInfo> &baseMedia,    QList<MediaInfo>& sdpMedia);

  MediaInfo copyMedia(const MediaInfo &media);

  void updateP2PTemplate(uint32_t sessionID, QList<MediaInfo>& medias);

  // returns whether the SSRC was found and set
  bool setCorrespondingSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                            int index, QList<MediaInfo>& medias);

  // generates the SSRC and sets it to media
  void generateSSRC(uint32_t sessionID, uint32_t mediaSessionID, MediaInfo& media);

  QString nextMID(uint32_t sessionID);
  void removeMID(MediaInfo& media);

  void updateMediaState(MediaInfo& currentState, const MediaInfo &newState);
  bool verifyMediaInfoMatch(const MediaInfo& currentState, const MediaInfo& newState) const;

  void handleSSRCUpdate(QList<QList<SDPAttribute>>& currentAttributes,
                        const QList<QList<SDPAttribute>>& newAttributes);

  void distributeGeneratedMedia(uint32_t sessionID, SDPMessageInfo &sdp);

  void updateP2PMedias(SDPMessageInfo &sdp);

  void distributeSFUSSRCs(uint32_t sessionID, SDPMessageInfo &sdp);

  ConferenceType type_;

  /* These are the templates used to form new connections between other participants that us.
   *
   * In mesh with multiplexing, there is no need to update the candidate ports and therefore
   * these templates can be used as is for each participant without further modifications.
   *
   * If multiplexing is not used in mesh, then the ports must be increased for each participant
   * that is added to the call.
   */

  // templates are used to construct an SDP message when a new participant joins
  std::map<uint32_t, QList<MediaInfo>> p2pSingleSDPTemplates_;
  std::map<uint32_t, QList<MediaInfo>> sfuSingleSDPTemplates_;

  // prepared messages correspond to what should be sent to existing participants
  std::map<uint32_t, QList<MediaInfo>> p2pPreparedMessages_;
  std::map<uint32_t, QList<MediaInfo>> sfuPreparedMessages_;

  // sometimes we need to beforehand generate an SSRC so we need to keep track of it
  struct GeneratedSSRC
  {
    uint32_t ssrc;
    QString  cname;
    uint32_t sessionID;
  };

  // first key is sessionID who this SSRC was sent to
  // the second key is mid
  std::unordered_map<uint32_t, std::unordered_map<QString, GeneratedSSRC>> generatedSSRCs_;

  std::unordered_map<uint32_t, QString> cnames_;
  std::unordered_map<uint32_t, QString> nextMID_;
};
