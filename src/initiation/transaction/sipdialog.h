#pragma once

#include <initiation/siptypes.h>

#include "sipdialogstate.h"
#include "sipdialogclient.h"
#include "sipserver.h"

#include <QString>
#include <QObject>

#include <memory>

/* This class holds all the information for managing one dialog.
 * Dialog is an established session with another user.
 * Dialogs are created with INVITE transaction and destroyed with BYE
 * This uses separate classes for client transactions and server transactions
 * as well as holding the current state of SIP dialog.
 */

class SIPDialog : public QObject
{
  Q_OBJECT
public:
  SIPDialog();

  void init(uint32_t sessionID, SIPTransactionUser *tu);

  void startCall(SIP_URI &address, QString localAddress, bool registered);
  void createDialogFromINVITE(std::shared_ptr<SIPMessageInfo> &invite, QString localAddress);

  void renegotiateCall();

  void acceptCall();
  void rejectCall();
  void endCall();
  void cancelCall();

  bool isThisYours(SIPRequest& request);
  bool isThisYours(SIPResponse& response);

  bool processRequest(SIPRequest& request);
  bool processResponse(SIPResponse& response);

signals:

  void dialogEnds(uint32_t sessionID);

  void sendRequest(uint32_t sessionID, SIPRequest& request);
  void sendResponse(uint32_t sessionID, SIPResponse& response);

private slots:

  void generateRequest(uint32_t sessionID, RequestType type);
  void generateResponse(uint32_t sessionID, ResponseType type);

private:

  SIPDialogState state_;
  SIPDialogClient client_;
  SIPServer server_;
};
