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

  std::shared_ptr<SDPMessageInfo> getSDP();

  // return NULL, if suitable could not be found.
  std::shared_ptr<SDPMessageInfo> modifyToSuitable(std::shared_ptr<SDPMessageInfo> suggestedSDP);

  bool isSDPSuitable(std::shared_ptr<SDPMessageInfo> suggestedSDP);

private:

  void generateSDP();

  QHostAddress localAddress_;
  QString localUsername_;

  std::shared_ptr<SDPMessageInfo> ourInfo_;

  uint16_t firstAvailablePort_;

  uint16_t maxPort_;
};
