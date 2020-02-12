#include "sipclient.h"

#pragma once


// Sending SIP Requests and processing of SIP responses that belong to a dialog.

class SIPDialogClient : public SIPClient
{
  Q_OBJECT
public:
  SIPDialogClient();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  virtual void getRequestMessageInfo(RequestType type,
                                     std::shared_ptr<SIPMessageInfo> &outMessage);

  virtual bool processResponse(SIPResponse& response,
                               SIPDialogState& state);

  // send a request
  bool startCall(QString callee);
  void requestEnd();
  void requestCancel();

  void requestRenegotiation();

protected:
  virtual void processTimeout();

  virtual bool startTransaction(RequestType type);

signals:
  // send messages to other end
  void sendDialogRequest(uint32_t sessionID, RequestType type);

  void destroyDialog();

private:
  uint32_t sessionID_;

  SIPTransactionUser* transactionUser_;
};
