#pragma once

#include "siptypes.h"

#include <QHostAddress>

#include <memory>

class GlobalSDPState
{
public:
  GlobalSDPState();

  void setLocalInfo(QHostAddress localAddress, QString username);
  void setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections);

  // when sending an invite, use this'
  // generates the next suitable SDP message
  std::shared_ptr<SDPMessageInfo> localInviteSDP();

  // checks if invite message is acceptable
  // returns NULL if suitable could not be found
  // chooces what to use
  std::shared_ptr<SDPMessageInfo> localResponseSDP(std::shared_ptr<SDPMessageInfo> remoteInviteSDP);
  std::shared_ptr<SDPMessageInfo> remoteFinalSDP(std::shared_ptr<SDPMessageInfo> remoteInviteSDP);

private:

  QHostAddress localAddress_;
  QString localUsername_;

  uint16_t firstAvailablePort_;

  uint16_t maxPort_;
};
