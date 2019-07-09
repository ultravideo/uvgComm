#pragma once

#include "initiation/negotiation/sdptypes.h"
#include "sdpparametermanager.h"
#include "ice.h"

#include <QHostAddress>
#include <QMutex>

#include <deque>
#include <memory>

// This class generates the SDP messages and is capable of checking if proposed SDP is suitable.

class GlobalSDPState
{
public:
  GlobalSDPState();

  void setLocalInfo(QString username);

  // Use this to generate the first SDP offer of the negotiation.
  // Includes all the media codecs suitable to us in preferred order.
  std::shared_ptr<SDPMessageInfo> generateInitialSDP(QHostAddress localAddress);

  // Use this to generate our response to their SDP suggestion.
  // Sets unacceptable media streams port number to 0.
  // Selects a subset of acceptable payload types from each media.
  // returns nullptr if suitable could not be found.
  std::shared_ptr<SDPMessageInfo> generateResponseSDP(SDPMessageInfo& sdpOffer,
                                                      QHostAddress localAddress,
                                                      std::shared_ptr<SDPMessageInfo> localSuggestion,
                                                      uint32_t sessionID);

  // check if their response is still valid.
  bool verifyResponseSDP(SDPMessageInfo& remoteInviteSDP, uint32_t sessionID);

  // frees the ports when they are not needed in rest of the program
  void endSession(std::shared_ptr<SDPMessageInfo> sessionSDP);

  void startICECandidateNegotiation(QList<std::shared_ptr<ICEInfo>>& local,
                                    QList<std::shared_ptr<ICEInfo>>& remote, uint32_t sessionID);

  void ICECleanup(uint32_t sessionID);

  // update the MediaInfo of remote and locals SDPs to include the nominated connections
  void updateFinalSDPs(SDPMessageInfo& localSDP, SDPMessageInfo& remoteSDP, uint32_t sessionID);

  bool canStartSession()
  {
    return parameters_.enoughFreePorts();
  }

private:

  // TODO: This should be moved to MediaManager.
  std::shared_ptr<SDPMessageInfo> generateSDP(QHostAddress localAddress);

  bool generateAudioMedia(MediaInfo &audio);
  bool generateVideoMedia(MediaInfo &video);

  // Checks if SDP is acceptable to us.
  bool checkSDPOffer(SDPMessageInfo& offer);

  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo);

  QString localUsername_;

  std::unique_ptr<ICE> ice_;

  SDPParameterManager parameters_;
};
