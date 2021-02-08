#pragma once

#include "initiation/transaction/sipclient.h"
#include "initiation/transaction/sipdialogstate.h"

#include "initiation/siptypes.h"

#include <QObject>
#include <QTimer>


class SIPNonDialogClient;
class SIPDialogState;
class SIPTransactionUser;
class ServerStatusView;

class SIPRegistration : public QObject
{
  Q_OBJECT
public:
  SIPRegistration();
  ~SIPRegistration();

  void init(ServerStatusView* statusView);

  // return if we can delete
  void uninit();

  void bindToServer(QString serverAddress, QString localAddress,
                    uint16_t port);

  // Identify if this reponse is to our REGISTER-request
  bool identifyRegistration(SIPResponse& response);

  void processNonDialogResponse(SIPResponse& response);

  void sendNonDialogRequest(SIPRequest& request, QVariant &content);

  bool haveWeRegistered();


signals:

  void transportProxyRequest(QString& address, SIPRequest& request);

private slots:

  void refreshRegistration();

private:

  enum RegistrationStatus {INACTIVE, FIRST_REGISTRATION,
                           RE_REGISTRATION, DEREGISTERING, REG_ACTIVE};

  SIPClient client_;
  SIPDialogState state_;

  QString contactAddress_;
  uint16_t contactPort_;

  QString serverAddress_;

  RegistrationStatus status_;

  ServerStatusView* statusView_;
  QTimer retryTimer_;
};
