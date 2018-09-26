#pragma once

//#include "sip/siptypes.h"
#include "sip/sdptypes.h"
#include "sdpparametermanager.h"

#include <QHostAddress>
#include <QMutex>

#include <deque>
#include <memory>

// This class holds the stat


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
                                                std::shared_ptr<SDPMessageInfo> localSuggestion = nullptr);

  // return if the final SDP was suitable. It should be, but just to be sure
  bool remoteFinalSDP(SDPMessageInfo& remoteInviteSDP);

  // frees the ports when they are not needed in rest of the program
  void endSession(std::shared_ptr<SDPMessageInfo> sessionSDP);

  bool canStartSession()
  {
    return parameters_.enoughFreePorts();
  }

private:

  // TODO: This should be moved to MediaManager.
  std::shared_ptr<SDPMessageInfo> generateSDP(QHostAddress localAddress);

  MediaInfo generateAudioMedia();
  MediaInfo generateVideoMedia();

  bool checkSDPOffer(SDPMessageInfo& offer);

  QString localUsername_;

  SDPParameterManager parameters_;
};
