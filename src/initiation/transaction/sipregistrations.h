#pragma once

#include "initiation/siptypes.h"

#include <QObject>
#include <QHostAddress>

class SIPNonDialogClient;
class SIPDialogState;
class SIPTransactionUser;

class SIPRegistrations : public QObject
{
  Q_OBJECT
public:
  SIPRegistrations();

  void init(SIPTransactionUser* callControl);

  void bindToServer(QString serverAddress, QHostAddress localAddress);

  bool identifyRegistration(SIPResponse response, QString address);

  void processNonDialogResponse();

  void sendNonDialogRequest(SIP_URI& uri, RequestType type);

signals:

  void transportProxyRequest(QString& address, SIPRequest& request);

private:

  struct SIPRegistrationData
  {
    std::shared_ptr<SIPNonDialogClient> client;
    std::shared_ptr<SIPDialogState> state;

    QHostAddress localAddress;
  };

  std::map<QString, SIPRegistrationData> registrations_;

  SIPTransactionUser* transactionUser_;
};
