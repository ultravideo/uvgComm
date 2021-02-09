#pragma once


#include "initiation/transaction/sipserver.h"
#include "initiation/transaction/sipclient.h"
#include "initiation/siptransactionuser.h"

#include "initiation/sipmessageprocessor.h"

#include <memory>

class SIPSingleCall : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPSingleCall();

  void init(std::shared_ptr<SIPClient> client,
            std::shared_ptr<SIPServer> server,
            SIPTransactionUser* tu,
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

  void transmitRequest(SIPRequest& request, QVariant& content);

  void transmitResponse(SIPResponse &response, QVariant& content);

  void processFailure(QString message);

private:

  std::shared_ptr<SIPClient> client_;
  std::shared_ptr<SIPServer> server_;

  SIPTransactionUser* transactionUser_;

  uint32_t sessionID_;

  // This variable is used to avoid HEAP corruption in a way that we would
  // delete ourselves.
  bool shouldLive_;
};
