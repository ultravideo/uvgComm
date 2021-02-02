#pragma once


#include "initiation/transaction/sipserver.h"
#include "initiation/transaction/sipclient.h"
#include "initiation/siptransactionuser.h"

#include <memory>

class SIPSingleCall : public QObject
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

  bool shouldBeKeptAlive()
  {
    return shouldLive_;
  }

signals:
  void sendDialogRequest(uint32_t sessionID, SIPRequest& request);


public slots:

  void processResponse(SIPRequestMethod originalRequest,
                       SIPResponseStatus status);

  void processFailure(QString message);

  void transmitRequest(SIPRequest& request);

private:

  std::shared_ptr<SIPClient> client_;
  std::shared_ptr<SIPServer> server_;

  SIPTransactionUser* transactionUser_;

  uint32_t sessionID_;

  // This variable is used to avoid HEAP corruption in a way that we would
  // delete ourselves.
  bool shouldLive_;
};
