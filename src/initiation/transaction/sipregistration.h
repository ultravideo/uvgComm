#pragma once

#include "initiation/sipmessageprocessor.h"
#include "initiation/siptypes.h"

#include <QObject>
#include <QTimer>


class SIPNonDialogClient;
class SIPDialogState;
class SIPTransactionUser;
class ServerStatusView;

/* Handles logic that is used for keeping registration up to date
 * with REGISTER-requests. */

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

  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

private slots:

  void refreshRegistration();

private:

  void sendREGISTERRequest(uint32_t expires);

  bool active_;

  QString serverAddress_;

  ServerStatusView* statusView_;
  QTimer retryTimer_;
};
