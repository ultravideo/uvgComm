#pragma once


#include "initiation/negotiation/sdptypes.h"

#include "initiation/sipmessageprocessor.h"

#include <QMutex>

#include <deque>
#include <memory>

/* This class generates the SDP messages and is capable of checking if proposed
 * SDP is suitable.

 * SDP in SIP is based on offer/answer model where one side sends an offer to
 * which the other side responds with an answer.

 * See RFC 3264 for details.
 */

class SDPMeshConference;

// State tells what is the next step for this sessionID.
// State is needed to accomidate software with different
// negotiation order from ours.
enum NegotiationState {NEG_NO_STATE,
                       NEG_OFFER_SENT,
                       NEG_OFFER_RECEIVED,
                       NEG_FINISHED,
                       NEG_FAILED};


class SDPNegotiation : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SDPNegotiation(uint32_t sessionID, QString localAddress, QString cname,
                 std::shared_ptr<SDPMessageInfo> localSDP,
                 std::shared_ptr<SDPMeshConference> sdpConf);

  void setBaseSDP(std::shared_ptr<SDPMessageInfo> localSDP);

  // frees the ports when they are not needed in rest of the program
  virtual void uninit();

public slots:

virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                    SIPResponseStatus generatedResponse);
virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                     bool retryRequest);

private:

  // When sending an SDP offer or answer
  bool sdpToContent(QVariant &content);

  // when receiving any SDP
  bool processSDP(QVariant &content);

  // When receiving an SDP offer
  bool processOfferSDP(QVariant &content);
  // When receiving an SDP answer
  bool processAnswerSDP(QVariant &content);

  // Is the internal state of this class correct for this sessionID
  bool checkSessionValidity(bool checkRemote) const;

  void addSDPAccept(std::shared_ptr<QList<SIPAccept>>& accepts);

  bool isSDPAccepted(std::shared_ptr<QList<SIPAccept>>& accepts);

  std::shared_ptr<SDPMessageInfo> findCommonSDP(const SDPMessageInfo &baseSDP,
                    const SDPMessageInfo &comparedSDP);

  bool selectBestCodec(const QList<uint8_t> &comparedNums, const QList<RTPMap> &comparedCodecs,
                       const QList<uint8_t> &baseNums,     const QList<RTPMap> &baseCodecs,
                             QList<uint8_t>& resultNums,         QList<RTPMap>& resultCodecs);

  bool matchMedia(std::vector<int>& matches,
                  const SDPMessageInfo &firstSDP,
                  const SDPMessageInfo& secondSDP);

  SDPAttributeType findStatusAttribute(const QList<SDPAttributeType>& attributes) const;

  void setSSRC(unsigned int mediaIndex, MediaInfo& media);
  void setMID(unsigned int mediaIndex, MediaInfo& media);


  uint32_t sessionID_;

  std::shared_ptr<SDPMessageInfo> localbaseSDP_;
  std::shared_ptr<SDPMessageInfo> localSDP_;
  std::shared_ptr<SDPMessageInfo> remoteSDP_;

  NegotiationState negotiationState_;

  // INVITE and INVITE OK tell us whether SDP is accepted by peer/us
  bool peerAcceptsSDP_;

  QString localAddress_;

  std::shared_ptr<SDPMeshConference> sdpConf_;

  QString cname_;

  std::unordered_map<unsigned int, uint32_t> mediaSSRCs_;
};
