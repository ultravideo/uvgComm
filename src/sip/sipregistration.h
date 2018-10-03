#pragma once

#include "sip/siptypes.h"

#include <QHostAddress>

/* This class is responsible for holding necessary SIP information
 * outside of dialog.
*/


class SIPRegistration
{
public:
  SIPRegistration(QString serverAddress);

  void initServer();

  void updateAddressBook();

  void setHost(QString location);

  std::shared_ptr<SIPMessageInfo> generateRegisterRequest(QString localAddress);

  bool isContactAtThisServer(QString serverAddress);
  bool isInitiated() const
  {
    return initiated_;
  }

private:

  void initLocalURI();
  ViaInfo getLocalVia(QString localAddress);

  SIP_URI localUri_;
  QString serverAddress_;

  bool initiated_;
  bool registered_;
};
