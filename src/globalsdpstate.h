#pragma once

//#include "sip/siptypes.h"
#include "sip/sdptypes.h"

#include <QHostAddress>
#include <QMutex>

#include <deque>
#include <memory>

class GlobalSDPState
{
public:
  GlobalSDPState();

  void setLocalInfo(QString username);
  void setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections);

  // when sending an invite, use this
  // generates the next suitable SDP message with all possible options
  std::shared_ptr<SDPMessageInfo> localSDPSuggestion(QHostAddress localAddress);

  // checks if invite message is acceptable
  // use for getting our answer and for getting our final to be used
  // returns NULL if suitable could not be found
  // chooces what to use
  std::shared_ptr<SDPMessageInfo> localFinalSDP(SDPMessageInfo& remoteSDP, QHostAddress localAddress,
                                                std::shared_ptr<SDPMessageInfo> localSuggestion = NULL);

  // return if the final SDP was suitable. It should be, but just to be sure
  bool remoteFinalSDP(SDPMessageInfo& remoteInviteSDP);

  // TODO: failed SDP negotiations and ended calls

private:

  // return the lower port of the pair and removes both from list of available ports
  uint16_t nextAvailablePortPair();
  void makePortPairAvailable(uint16_t lowerPort);

  // TODO: This should be moved to MediaManager.
  std::shared_ptr<SDPMessageInfo> generateSDP(QHostAddress localAddress);

  bool checkSDPOffer(SDPMessageInfo& offer);

  QString localUsername_;

  uint16_t remainingPorts_;

  QMutex portLock_;
  //keeps a list of all available ports. Has only every other port because of rtcp
  std::deque<uint16_t> availablePorts_;
};
