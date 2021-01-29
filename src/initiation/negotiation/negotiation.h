#pragma once

#include "sdpnegotiator.h"
#include "ice.h"
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
                       NEG_OFFER_GENERATED,
                       NEG_ANSWER_GENERATED,
                       NEG_FINISHED};


class Negotiation : public SIPMessageProcessor
{
  Q_OBJECT
public:
  Negotiation(std::shared_ptr<NetworkCandidates> candidates,
              uint32_t sessionID);

  // frees the ports when they are not needed in rest of the program
  void endSession();

  // call these only after the corresponding SDP has been generated
  std::shared_ptr<SDPMessageInfo> getLocalSDP() const;
  std::shared_ptr<SDPMessageInfo> getRemoteSDP() const;

public slots:

// TODO: Remove sessionID and localaddress from parameters
virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
virtual void processOutgoingResponse(SIPResponse& response, QVariant& content,
                                     QString localAddress);

virtual void processIncomingRequest(SIPRequest& request, QVariant& content, QString localAddress);
virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                     QString localAddress);

signals:
  void iceNominationSucceeded(quint32 sessionID);
  void iceNominationFailed(quint32 sessionID);

public slots:
  void nominationSucceeded();
  void nominationFailed();

private:

  // When sending an SDP offer
  bool SDPOfferToContent(QVariant &content, QString localAddress);
  // When receiving an SDP offer
  bool processOfferSDP(QVariant &content, QString localAddress);
  // When sending an SDP answer
  bool SDPAnswerToContent(QVariant &content);
  // When receiving an SDP answer
  bool processAnswerSDP(QVariant &content);

  // Use this to generate the first SDP offer of the negotiation.
  // Includes all the media codecs suitable to us in preferred order.
  bool generateOfferSDP(QString localAddress);

  // Use this to generate our response to their SDP suggestion.
  // Sets unacceptable media streams port number to 0.
  // Selects a subset of acceptable payload types from each media.
  bool generateAnswerSDP(SDPMessageInfo& remoteSDPOffer,
                         QString localAddress);

  // process their response SDP.
  bool processAnswerSDP(SDPMessageInfo& remoteSDPAnswer);


  // Is the internal state of this class correct for this sessionID
  bool checkSessionValidity(bool checkRemote) const;

  std::shared_ptr<NetworkCandidates> nCandidates_;
  std::unique_ptr<ICE> ice_;

  std::shared_ptr<SDPMessageInfo> localSDP_;
  std::shared_ptr<SDPMessageInfo> remoteSDP_;

  NegotiationState negotiationState_;

  SDPNegotiator negotiator_;

  uint32_t sessionID_;
};
