#pragma once

#include "initiation/negotiation/sdptypes.h"
#include "sdpparametermanager.h"
#include "ice.h"

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

class Negotiation : public QObject
{
  Q_OBJECT
public:
  Negotiation();

  void setLocalInfo(QString username);

  // Use this to generate the first SDP offer of the negotiation.
  // Includes all the media codecs suitable to us in preferred order.
  bool generateOfferSDP(QString localAddress, uint32_t sessionID);

  // Use this to generate our response to their SDP suggestion.
  // Sets unacceptable media streams port number to 0.
  // Selects a subset of acceptable payload types from each media.
  bool generateAnswerSDP(SDPMessageInfo& remoteSDPOffer,
                         QString localAddress,
                         uint32_t sessionID);

  // process their response SDP.
  bool processAnswerSDP(SDPMessageInfo& remoteSDPAnswer, uint32_t sessionID);

  // frees the ports when they are not needed in rest of the program
  void endSession(uint32_t sessionID);

  void endAllSessions();

  void startICECandidateNegotiation(uint32_t sessionID);
  void respondToICECandidateNominations(uint32_t sessionID);

  // update the MediaInfo of remote and locals SDPs to include the nominated connections
  void setICEPorts(uint32_t sessionID);

  bool canStartSession()
  {
    return parameters_.enoughFreePorts();
  }

  // call these only after the corresponding SDP has been generated
  std::shared_ptr<SDPMessageInfo> getLocalSDP(uint32_t sessionID) const;
  std::shared_ptr<SDPMessageInfo> getRemoteSDP(uint32_t sessionID,
                                               QList<uint16_t> &sendPorts) const;

  NegotiationState getState(uint32_t sessionID);

signals:
  void iceNominationSucceeded(quint32 sessionID);
  void iceNominationFailed(quint32 sessionID);

public slots:
  void nominationSucceeded(quint32 sessionID);

private:

  std::shared_ptr<SDPMessageInfo> generateLocalSDP(QString localAddress);

  std::shared_ptr<SDPMessageInfo> negotiateSDP(SDPMessageInfo& remoteSDPOffer,
                                               QString localAddress);

  void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress);
  void setConnectionAddress(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress);

  bool generateAudioMedia(MediaInfo &audio);
  bool generateVideoMedia(MediaInfo &video);

  bool selectBestCodec(QList<uint8_t>& remoteNums,       QList<RTPMap>& remoteCodecs,
                       QList<uint8_t>& supportedNums,    QList<RTPMap>& supportedCodecs,
                       QList<uint8_t>& outMatchingNums,  QList<RTPMap>& outMatchingCodecs);

  // Checks if SDP is acceptable to us.
  bool checkSDPOffer(SDPMessageInfo& offer);

  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo);

  // Is the internal state of this class correct for this sessionID
  bool checkSessionValidity(uint32_t sessionID, bool checkRemote) const;

  QString localUsername_;

  std::unique_ptr<ICE> ice_;

  SDPParameterManager parameters_;

  struct CallParameters
  {
    std::shared_ptr<SDPMessageInfo> localSDP;
    std::shared_ptr<SDPMessageInfo> remoteSDP;
    QList<uint16_t> sendports;
  };

  // maps sessionID to pair of SDP:s. Local and remote in that order.
  std::map<uint32_t, CallParameters> sdps_;

  std::map<uint32_t, NegotiationState> negotiationStates_;
};
