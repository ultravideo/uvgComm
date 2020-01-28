#include "sipclienttransaction.h"

#pragma once


// Sending SIP Requests and processing of SIP responses that belong to a dialog.

class SIPDialogClient : public SIPClientTransaction
{
  Q_OBJECT
public:
  SIPDialogClient(SIPTransactionUser* tu);

  void setSessionID(uint32_t sessionID);

  virtual void getRequestMessageInfo(RequestType type,
                                     std::shared_ptr<SIPMessageInfo> &outMessage);

  virtual bool processResponse(SIPResponse& response,
                               std::shared_ptr<SIPDialogState> state);

  // send a request
  bool startCall(QString callee);
  void endCall();
  void cancelCall();

  void renegotiateCall();

protected:
  virtual void processTimeout();

  virtual void startTransaction(RequestType type);

signals:
  // send messages to other end
  void sendDialogRequest(uint32_t sessionID, RequestType type);

  void destroyDialog();

private:
  uint32_t sessionID_;
};
