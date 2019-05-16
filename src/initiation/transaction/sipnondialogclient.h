#pragma once


#include "sipclienttransaction.h"

class SIPNonDialogClient : public SIPClientTransaction
{
  Q_OBJECT
public:
  SIPNonDialogClient(SIPTransactionUser* tu);

  void set_remoteURI(SIP_URI& uri);

  virtual bool processResponse(SIPResponse& response);

  virtual void sendRequest(RequestType type);

  void registerToServer();

signals:

  void sendNonDialogRequest(SIP_URI uri, RequestType type);

private:

  SIP_URI remoteUri_;
};
