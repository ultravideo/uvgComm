#pragma once

#include "sip/sdptypes.h"
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

  // when sending an invite, use this
  // generates the next suitable SDP message with all possible options
  std::shared_ptr<SDPMessageInfo> localSDPSuggestion(QHostAddress localAddress);

  // checks if invite message is acceptable
  // use for getting our answer and for getting our final to be used
  // returns nullptr if suitable could not be found
  // chooces what to use
  std::shared_ptr<SDPMessageInfo> localFinalSDP(SDPMessageInfo& remoteSDP, QHostAddress localAddress,
                                                std::shared_ptr<SDPMessageInfo> localSuggestion, uint32_t sessionID);

  // return if the final SDP was suitable. It should be, but just to be sure
  bool remoteFinalSDP(SDPMessageInfo& remoteInviteSDP, uint32_t sessionID);

  // frees the ports when they are not needed in rest of the program
  void endSession(std::shared_ptr<SDPMessageInfo> sessionSDP);

  void startICECandidateNegotiation(QList<std::shared_ptr<ICEInfo>>& local, QList<std::shared_ptr<ICEInfo>>& remote, uint32_t sessionID);

  void ICECleanup(uint32_t sessionID);

  // update the MediaInfo of remote and locals SDPs to include the nominated connections
  void updateFinalSDPs(SDPMessageInfo& localSDP, SDPMessageInfo& remoteSDP, uint32_t sessionID);

  bool canStartSession()
  {
    return parameters_.enoughFreePorts();
  }

private:

  // TODO: This should be moved to MediaManager.
  std::shared_ptr<SDPMessageInfo> generateSDP(QHostAddress localAddress, QList<std::shared_ptr<ICEInfo>> *remoteCandidates);

  bool generateAudioMedia(MediaInfo &audio);
  bool generateVideoMedia(MediaInfo &video);

  bool checkSDPOffer(SDPMessageInfo& offer);

  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo);

  QString localUsername_;

  std::unique_ptr<ICE> ice_;

  SDPParameterManager parameters_;
};
