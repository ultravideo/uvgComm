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

  // Identify if this reponse is to our REGISTER-request
  bool identifyRegistration(SIPResponse& response, QString &outAddress);

  void processNonDialogResponse(SIPResponse& response);

  void sendNonDialogRequest(SIP_URI& uri, RequestType type);

  bool haveWeRegistered();

signals:

  void transportProxyRequest(QString& address, SIPRequest& request);

private:

  struct SIPRegistrationData
  {
    std::shared_ptr<SIPNonDialogClient> client;
    std::shared_ptr<SIPDialogState> state;

    QHostAddress localAddress;

    bool active;

    // TODO: refresh the registration
  };

  std::map<QString, SIPRegistrationData> registrations_;

  SIPTransactionUser* transactionUser_;
};
