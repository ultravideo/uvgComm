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

  // constructs the SIP message info struct as much as possible
  void getRequestMessageInfo(RequestType type,
                             std::shared_ptr<SIPMessageInfo> &outMessage);

  virtual bool processResponse(SIPResponse& response,
                               std::shared_ptr<SIPDialogState> state);

  virtual void startTransaction(RequestType type);

  void registerToServer();


private:

  SIP_URI remoteUri_;
};
