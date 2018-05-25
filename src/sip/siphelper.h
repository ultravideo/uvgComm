#pragma once

#include "sip/siptypes.h"

#include <QHostAddress>

/* This class is responsible for holding necessary SIP information
 * outside of dialog.
*/


class SIPHelper
{
public:
  SIPHelper();

  bool isInitiated() const
  {
    return initiated_;
  }

  void initServer(SIP_URI remoteUri);
  void initPeerToPeer(SIP_URI remoteUri);

  void setServerLocation(QString location);
  QString getServerLocation();

  bool isAllowedUser(SIP_URI user);

  std::shared_ptr<SIPMessageInfo> generateMessageBase(QString localAddress);

  // REGISTER and INVITE are non-dialog requests
  void generateNonDialogRequest(std::shared_ptr<SIPMessageInfo> messageBase);

private:

  void initLocalURI();
  ViaInfo getLocalVia(QString localAddress);

  SIP_URI localUri_;
  SIP_URI remoteUri_;

  bool initiated_;
};
