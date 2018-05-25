#pragma once

#include "sip/siptypes.h"

#include <QHostAddress>

/* This class is responsible for holding necessary SIP information
 * outside of dialog.
*/


class SIPRegistration
{
public:
  SIPRegistration();

  void initServer();

  void setHost(QString location);

  std::shared_ptr<SIPMessageInfo> generateRegisterRequest(QString localAddress);

  bool isInitiated() const
  {
    return initiated_;
  }

private:

  void initLocalURI();

  ViaInfo getLocalVia(QString localAddress);

  SIP_URI localUri_;

  bool initiated_;
  bool registered_;
};
