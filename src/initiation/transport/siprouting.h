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


  // check the rport value if
  void processResponseViaFields(QList<ViaInfo> &vias,
                                QString localAddress,
                                uint16_t localPort);

  // sets the via and contact addresses of the request
  void getViaAndContact(std::shared_ptr<SIPMessageBody> message,
                         QString localAddress,
                         uint16_t localPort);

  // modifies the just the contact-address. Use with responses
  void getContactAddress(std::shared_ptr<SIPMessageBody> message,
                          QString localAddress,
                          uint16_t localPort, SIPType type);

private:

  QString contactAddress_;
  uint16_t contactPort_;

  bool first_;
};
