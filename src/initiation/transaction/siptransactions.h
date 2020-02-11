#pragma once

#include "initiation/transaction/sipnondialogclient.h"
#include "initiation/transport/siptransport.h"

#include "common.h"

#include <QMap>


/* SIP in Kvazzup follows the Transport, Transaction, Transaction User principle.
 * This class represents the SIP Transaction layer. It uses separate classes for
 * client transactions and server transactions as well as holding the current state
 * of SIP dialog
 */

/* Some terms from RFC 3621:
 * Dialog = a SIP dialog constructed with INVITE-transaction
 * Session = a media session negotiated in INVITE-transaction
 */

// Contact is basically the same as SIP_URI
struct Contact
{
  QString username;
  QString realName;
  QString remoteAddress;
};

class SIPDialogState;
class SIPTransactionUser;
class SIPServerTransaction;
class SIPDialogClient;

class SIPTransactions : public QObject
{
  Q_OBJECT
public:
  SIPTransactions();
  
  void init(SIPTransactionUser* callControl);
  void uninit();

  // reserve sessionID for a future call
  uint32_t reserveSessionID();

  // start a call with address. Returns generated sessionID
  void startCall(Contact& address, QString localAddress,
                 uint32_t sessionID, bool registered);

  // sends a re-INVITE
  void renegotiateCall(uint32_t sessionID);

  void renegotiateAllCalls();

  // transaction user wants something.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);

  void endCall(uint32_t sessionID);
  void endAllCalls();

  void failedToSendMessage();

  // returns true if the identification was successful
  bool identifySession(SIPRequest request, QString localAddress,
                       uint32_t& out_sessionID);

  bool identifySession(SIPResponse response,
                       uint32_t& out_sessionID);

  // when sip connection has received a request/response it is handled here.
  // TODO: some sort of SDP situation should also be included.
  void processSIPRequest(SIPRequest request, uint32_t sessionID);

  void processSIPResponse(SIPResponse response, uint32_t sessionID);

signals:

  void transportRequest(uint32_t sessionID, SIPRequest &request);
  void transportResponse(uint32_t sessionID, SIPResponse &response);

private slots:

  void sendDialogRequest(uint32_t sessionID, RequestType type);
  void sendResponse(uint32_t sessionID, ResponseType type);

private:

  enum CallConnectionType {PEERTOPEER, SIPSERVER, CONTACT};

  struct SIPDialogData
  {
    std::shared_ptr<SIPDialogState> state;
    // do not stop connection before responding to all requests
    std::shared_ptr<SIPServerTransaction> server;
    std::shared_ptr<SIPDialogClient> client;

    QTimer endTimer;
  };

  uint32_t createDialogFromINVITE(QString localAddress,
                                  std::shared_ptr<SIPMessageInfo> &invite);
  void createBaseDialog(uint32_t sessionID,
                        std::shared_ptr<SIPDialogData>& dialog);
  void destroyDialog(uint32_t sessionID);
  void removeDialog(uint32_t sessionID);

  // This mutex makes sure that the dialog has been added to the dialogs_ list
  // before we are accessing it when receiving messages
  QMutex dialogMutex_;

  QMutex pendingConnectionMutex_;

  // SessionID:s are used in this program to keep track of dialogs.
  // The CallID is not used because we could be calling ourselves
  // and using uint32_t is simpler than keeping track of tags.

  // TODO: sessionID should be called dialogID

  uint32_t nextSessionID_;
  std::map<uint32_t, std::shared_ptr<SIPDialogData>> dialogs_;

  SIPTransactionUser* transactionUser_;
};
