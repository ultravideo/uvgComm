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
  void init(SIPTransactionUser* callControl, SIPTransactions& transactions);
  void uninit(SIPTransactions& transactions);

private:

  ConnectionServer tcpServer_;

  uint16_t sipPort_;
};
