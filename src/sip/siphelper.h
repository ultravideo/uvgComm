#pragma once

#include "sip/siptypes.h"

#include <QHostAddress>

/* This class is responsible for holding necessary SIP information
 * outside of dialog. This info is mostly gotten from
*/

class SIPHelper
{
public:
  SIPHelper();

  void init();

  QString getServerLocation();

  ViaInfo getLocalAddress();

  // use set/get remote URI when initializing a dialog
  void setNextRemoteURI(SIP_URI remoteUri);
  SIP_URI getNextRemoteURI();

  SIP_URI getLocalURI();
  SIP_URI getLocalContactURI();

  bool isForbiddenUser(SIP_URI user);

  std::shared_ptr<SIPMessageInfo> getMessageBase();

private:

  SIP_URI localUri_;
  SIP_URI nextRemoteUri_;

  QString serverLocation_;
};
