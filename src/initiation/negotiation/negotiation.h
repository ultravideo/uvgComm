#pragma once

#include "initiation/negotiation/sdptypes.h"
#include "sdpparametermanager.h"
#include "ice.h"

#include <QHostAddress>
#include <QMutex>

#include <deque>
#include <memory>

// This class generates the SDP messages and is capable of checking if proposed
// SDP is suitable.

// SDP in SIP is based on offer/answer model where one side sends an offer to
// which the other side responds with an asnwer.

class Negotiation
{
public:
  Negotiation();

  void setLocalInfo(QString username);

  // Use this to generate the first SDP offer of the negotiation.
  // Includes all the media codecs suitable to us in preferred order.
  bool generateOfferSDP(QHostAddress localAddress, uint32_t sessionID);

  // Use this to generate our response to their SDP suggestion.
  // Sets unacceptable media streams port number to 0.
  // Selects a subset of acceptable payload types from each media.
  // TODO: Do as described
  bool generateAnswerSDP(SDPMessageInfo& remoteSDPOffer,
                         QHostAddress localAddress,
                         uint32_t sessionID);

  // process their response SDP.
  bool processAnswerSDP(SDPMessageInfo& remoteSDPAnswer, uint32_t sessionID);

  // frees the ports when they are not needed in rest of the program
  void endSession(uint32_t sessionID);

  void startICECandidateNegotiation(uint32_t sessionID);

  // update the MediaInfo of remote and locals SDPs to include the nominated connections
  void setICEPorts(uint32_t sessionID);

  bool canStartSession()
  {
    return parameters_.enoughFreePorts();
  }

  // call these only after the corresponding SDP has been generated
  std::shared_ptr<SDPMessageInfo> getLocalSDP(uint32_t sessionID) const;
  std::shared_ptr<SDPMessageInfo> getRemoteSDP(uint32_t sessionID) const;

private:

  std::shared_ptr<SDPMessageInfo> generateSDP(QHostAddress localAddress);

  bool generateAudioMedia(MediaInfo &audio);
  bool generateVideoMedia(MediaInfo &video);

  // Checks if SDP is acceptable to us.
  bool checkSDPOffer(SDPMessageInfo& offer);

  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo);

  // Is the internal state of this class correct for this sessionID
  bool checkSessionValidity(uint32_t sessionID, bool remotePresent) const;

  QString localUsername_;

  std::unique_ptr<ICE> ice_;

  SDPParameterManager parameters_;

  // maps sessionID to pair of SDP:s. Local and remote in that order.
  std::map<uint32_t, std::pair<std::shared_ptr<SDPMessageInfo>,std::shared_ptr<SDPMessageInfo> > > sdps_;
};
