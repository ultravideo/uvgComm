#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <QObject>
#include <QTimer>


class SIPNonDialogClient;
class SIPDialogState;
class SIPTransactionUser;
class ServerStatusView;

class SIPRegistration : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPRegistration();
  ~SIPRegistration();

  void init(ServerStatusView* statusView);

  // return if we can delete
  void uninit();

  void bindToServer(NameAddr& addressRecord, QString localAddress,
                    uint16_t port);

  void processNonDialogResponse(SIPResponse& response);

  void sendNonDialogRequest(SIPRequest& request, QVariant &content);

  bool haveWeRegistered();

  QString getServerAddress()
  {
    return serverAddress_;
  }

public slots:

  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);

private slots:

  void refreshRegistration();

private:
  enum RegistrationStatus {INACTIVE, FIRST_REGISTRATION,
                           RE_REGISTRATION, DEREGISTERING, REG_ACTIVE};

  void sendREGISTERRequest(uint32_t expires, RegistrationStatus newStatus);

  // TODO: these addressses should probably be the responsibility of routing
  QString contactAddress_;
  uint16_t contactPort_;

  QString serverAddress_;

  RegistrationStatus status_;

  ServerStatusView* statusView_;
  QTimer retryTimer_;

  bool attemptedAuth_;
};
