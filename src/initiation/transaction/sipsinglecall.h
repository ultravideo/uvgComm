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

  void acceptIncomingCall();
  void declineIncomingCall();

  bool shouldBeKeptAlive()
  {
    return shouldLive_;
  }

signals:
  void sendDialogRequest(uint32_t sessionID, SIPRequest& request);

  void sendResponse(uint32_t sessionID, SIPResponse& response);


public slots:

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

  void processFailure(QString message);

private:

  SIPRequest createRequest(SIPRequestMethod method);
  SIPResponse createResponse(SIPResponseStatus status);

  void setExpires(uint32_t timeout, std::shared_ptr<SIPMessageHeader> header);

  SIPTransactionUser* transactionUser_;

  uint32_t sessionID_;

  // This variable is used to avoid HEAP corruption in a way that we would
  // delete ourselves.
  bool shouldLive_;
};
