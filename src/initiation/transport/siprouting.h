#pragma once

#include "initiation/siptypes.h"

#include <QString>

#include <memory>

// This class is responsible for via and contact fields of SIP Requests
// and possible contact fields of responses

class SIPRouting
{
public:
  SIPRouting();

  // set our via address when we know what is our address
  void connectionEstablished(QString localAddress, uint16_t localPort);

  // check the rport value if
  void processResponseViaFields(QList<ViaInfo> &vias);

  // sets the via and contact addresses of the request
  void getRequestRouting(std::shared_ptr<SIPMessageInfo> message);

  // set response contact-address
  void getResponseContact(std::shared_ptr<SIPMessageInfo> message);

private:

  QString viaAddress_;
  uint16_t viaPort_;

  QString contactAddress;
  uint16_t contactPort_;
};
