#pragma once

#include "initiation/siptransactionuser.h"
#include "initiation/sipmessageprocessor.h"

#include <memory>

class SIPSingleCall : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPSingleCall();

  void init(SIPTransactionUser* tu,
            uint32_t sessionID);

  void startCall(QString callee);
  void endCall();
  void cancelOutgoingCall();

  bool shouldBeKeptAlive()
  {
    return shouldLive_;
  }

public slots:

  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

  void processFailure(QString message);

private:

  SIPRequest createRequest(SIPRequestMethod method);

  void setExpires(uint32_t timeout, std::shared_ptr<SIPMessageHeader> header);

  SIPTransactionUser* transactionUser_;

  uint32_t sessionID_;

  // This variable is used to avoid HEAP corruption in a way that we would
  // delete ourselves.
  bool shouldLive_;

  bool triedAuthenticating_;
};
