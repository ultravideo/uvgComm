#pragma once

#include "common.h"

#include <QHostAddress>

#include <memory>

class SDPState
{
public:
  SDPState();

  void init(QHostAddress localAddress, QString username, uint16_t firstAvailablePort);

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
};
