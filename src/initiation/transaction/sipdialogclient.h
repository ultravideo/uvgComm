#include "sipclienttransaction.h"

#pragma once


class SIPDialogClient : public SIPClientTransaction
{
  Q_OBJECT
public:
  SIPDialogClient(SIPTransactionUser* tu);

  void setSessionID(uint32_t sessionID);

  virtual bool processResponse(SIPResponse& response);

  // send a request
  bool startCall(QString callee);
  void endCall();
  void cancelCall();

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
