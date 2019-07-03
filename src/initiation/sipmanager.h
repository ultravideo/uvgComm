#pragma once

#include "initiation/transport/connectionserver.h"
#include "initiation/transaction/siptransactions.h"

class SIPTransactionUser;


class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  // start listening to incoming
  void init(SIPTransactionUser* callControl);
  void uninit();

  // REGISTER our information to SIP-registrar
  void bindToServer();

  // start a call with address. Returns generated sessionID
  uint32_t startCall(Contact& address);

  // transaction user wants something.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);

  void endCall(uint32_t sessionID);
  void endAllCalls();

  void getSDPs(uint32_t sessionID,
               std::shared_ptr<SDPMessageInfo>& localSDP,
               std::shared_ptr<SDPMessageInfo>& remoteSDP);

private:

  ConnectionServer tcpServer_;

  uint16_t sipPort_;

  SIPTransactions transactions_;
};
