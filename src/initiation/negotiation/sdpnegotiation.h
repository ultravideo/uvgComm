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
  SDPNegotiation(std::shared_ptr<SDPMessageInfo> localSDP);

  // frees the ports when they are not needed in rest of the program
  virtual void uninit();

public slots:

virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                    SIPResponseStatus generatedResponse);
virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                     bool retryRequest);

signals:
  void iceNominationSucceeded(const quint32 sessionID,
                              const std::shared_ptr<SDPMessageInfo> local,
                              const std::shared_ptr<SDPMessageInfo> remote);

  void iceNominationFailed(quint32 sessionID);

public slots:
  void nominationSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
                           quint32 sessionID);

private:

  // When sending an SDP offer or answer
  bool sdpToContent(std::shared_ptr<SDPMessageInfo> sdp, QVariant &content);

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


  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local);

  bool matchMedia(std::vector<int>& matches,
                  const SDPMessageInfo &firstSDP,
                  const SDPMessageInfo& secondSDP);

  SDPAttributeType findStatusAttribute(const QList<SDPAttributeType>& attributes) const;

  std::shared_ptr<SDPMessageInfo> localbaseSDP_;
  std::shared_ptr<SDPMessageInfo> localSDP_;
  std::shared_ptr<SDPMessageInfo> remoteSDP_;

  NegotiationState negotiationState_;

  // INVITE and INVITE OK tell us whether SDP is accepted by peer/us
  bool peerAcceptsSDP_;
};
