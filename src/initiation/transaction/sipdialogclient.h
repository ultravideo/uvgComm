#include "sipclienttransaction.h"

#pragma once


// Sending SIP Requests and processing of SIP responses that belong to a dialog.

class SIPDialogClient : public SIPClientTransaction
{
  Q_OBJECT
public:
  SIPDialogClient(SIPTransactionUser* tu);

  void setSessionID(uint32_t sessionID);

  virtual bool processResponse(SIPResponse& response,
                               bool inSessionActive,
                               bool& outSessionActivated);

  // send a request
  bool startCall(QString callee);
  void endCall();
  void cancelCall();

  void renegotiateCall();

protected:
  virtual void processTimeout();

  virtual void sendRequest(RequestType type);

signals:
  // send messages to other end
  void sendDialogRequest(uint32_t sessionID, RequestType type);

  void destroyDialog();

private:
  uint32_t sessionID_;
};
