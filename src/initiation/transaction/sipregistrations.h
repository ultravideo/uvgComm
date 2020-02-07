#pragma once

#include "initiation/siptypes.h"

#include <QObject>
#include <QTimer>


class SIPNonDialogClient;
class SIPDialogState;
class SIPTransactionUser;
class ServerStatusView;

class SIPRegistrations : public QObject
{
  Q_OBJECT
public:
  SIPRegistrations();

  void init(SIPTransactionUser* callControl, ServerStatusView* statusView);

  // return if we can delete
  void uninit();

  void bindToServer(QString serverAddress, QString localAddress,
                    uint16_t port);

  // Identify if this reponse is to our REGISTER-request
  bool identifyRegistration(SIPResponse& response, QString &outAddress);

  void processNonDialogResponse(SIPResponse& response);

  void sendNonDialogRequest(SIP_URI& uri, RequestType type);

  bool haveWeRegistered();


signals:

  void transportProxyRequest(QString& address, SIPRequest& request);

private slots:

  void refreshRegistrations();

private:

  struct SIPRegistrationData
  {
    std::shared_ptr<SIPNonDialogClient> client;
    std::shared_ptr<SIPDialogState> state;

    QString contactAddress;
    uint16_t contactPort;

    bool active;
    bool updatedContact;
  };

  std::map<QString, SIPRegistrationData> registrations_;

  SIPTransactionUser* transactionUser_;
  ServerStatusView* statusView_;

 QTimer retryTimer_;
};
