#include "sipclient.h"

#pragma once


// Sending SIP Requests and processing of SIP responses that belong to a dialog.

class SIPDialogClient : public SIPClient
{
  Q_OBJECT
public:
  SIPDialogClient();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  virtual bool processResponse(SIPResponse& response,
                               SIPDialogState& state);

  // send a request
  bool startCall(QString callee);
  void requestEnd();
  void requestCancel();

  void requestRenegotiation();

protected:
  virtual void processTimeout();

  virtual bool startTransaction(SIPRequestMethod type);

  virtual void byeTimeout();

signals:
  // send messages to other end
  void sendDialogRequest(uint32_t sessionID, SIPRequestMethod type);

  void BYETimeout(uint32_t sessionID);

private:
  uint32_t sessionID_;

  SIPTransactionUser* transactionUser_;
};
