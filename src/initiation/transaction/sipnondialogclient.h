#pragma once

#include "sipclient.h"

// Sending SIP Requests and processing of SIP responses that don't belong to
// a dialog. Typical example is the REGISTER-method. OPTIONS can also sent
// without a dialog.


class SIPNonDialogClient : public SIPClient
{
  Q_OBJECT
public:
  SIPNonDialogClient();
  ~SIPNonDialogClient();

  void set_remoteURI(SIP_URI& uri);

  // constructs the SIP message info struct as much as possible
  virtual void getRequestMessageInfo(SIPRequestMethod type,
                             std::shared_ptr<SIPMessageBody> &outMessage);

  virtual bool processResponse(SIPResponse& response,
                               SIPDialogState& state);

  void registerToServer();
  void unRegister();

signals:
  void sendNondialogRequest(SIP_URI& uri, SIPRequestMethod type);

private:

  SIP_URI remoteUri_;

  unsigned int expires_;
};
