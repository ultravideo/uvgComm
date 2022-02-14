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
                       NEG_OFFER_GENERATED,
                       NEG_ANSWER_GENERATED,
                       NEG_FINISHED};


class SDPNegotiation : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SDPNegotiation(QString localAddress);

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

  void addSDPAccept(std::shared_ptr<QList<SIPAccept>>& accepts);

  bool isSDPAccepted(std::shared_ptr<QList<SIPAccept>>& accepts);

  std::shared_ptr<SDPMessageInfo> localSDP_;
  std::shared_ptr<SDPMessageInfo> remoteSDP_;

  NegotiationState negotiationState_;

  // used to set the initial connection address in SDP. This only for spec,
  // the actual addresses used are determined by ICE.
  QString localAddress_;

  // INVITE and INVITE OK tell us whether SDP is accepted by peer/us
  bool peerAcceptsSDP_;
};
