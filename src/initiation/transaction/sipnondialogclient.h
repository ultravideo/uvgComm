#pragma once

#include "sipclienttransaction.h"

// Sending SIP Requests and processing of SIP responses that don't belong to
// a dialog. Typical example is the REGISTER-method. OPTIONS can also sent
// without a dialog.


class SIPNonDialogClient : public SIPClientTransaction
{
  Q_OBJECT
public:
  SIPNonDialogClient(SIPTransactionUser* tu);

  void set_remoteURI(SIP_URI& uri);

  virtual bool processResponse(SIPResponse& response,
                               bool inSessionActive,
                               bool &outSessionActivated);

  virtual void sendRequest(RequestType type);

  void registerToServer();

signals:

  void sendNonDialogRequest(SIP_URI uri, RequestType type);

private:

  SIP_URI remoteUri_;
};
