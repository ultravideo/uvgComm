#pragma once

#include "initiation/negotiation/globalsdpstate.h"
#include "initiation/transaction/sipnondialogclient.h"
#include "initiation/transport/siptransport.h"


#include "common.h"

#include <QMap>


/* This class manages the interactions between different components in
 * a SIP transaction. This class should implement as little functionality
 * as possible and only focus on interractions.
 */

/* Some terms from RFC 3621:
 * Dialog = a SIP dialog constructed with INVITE-transaction
 * Session = a media session negotiated in INVITE-transaction
 *
 * SIP in Kvazzup follows the Transport, Transaction, Transaction User principle.
 * This class represents the transaction layer.
 */

// TODO: some kind of address book class might be useful
// because the connection addresses can change.

// TODO: This class might be more versatile if it only had one transaction in it
// and SIPManager would manage all transactions. The question who recognizes
// which session a message belongs to should be in this case taken into consideration.

// Contact is basically the same as SIP_URI
struct Contact
{
  QString username;
  QString realName;
  QString remoteAddress;
};

class SIPRouting;
class SIPDialogState;
class SIPTransactionUser;
class SIPServerTransaction;
class SIPDialogClient;

class SIPTransactions : public QObject
{
  Q_OBJECT
public:
  SIPTransactions();
  
  // start listening to incoming 
  void init(SIPTransactionUser* callControl);
  void uninit();

  void bindToServer(QString serverAddress, QHostAddress localAddress, uint32_t sessionID);

  bool locatedAtServer(Contact& query) const
  {
    return registrations_.find(query.remoteAddress) != registrations_.end();
  }

  // reserve sessionID for a future call
  uint32_t reserveSessionID()
  {
    ++nextSessionID_;
    return nextSessionID_ - 1;
  }

  // start a call with address. Returns generated sessionID
  void startDirectCall(Contact& address, QHostAddress localAddress,
                       uint32_t sessionID);

  // TODO: not implemented
  void startProxyCall(Contact& address, QHostAddress localAddress,
                      uint32_t sessionID);

  // transaction user wants something.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);

  void endCall(uint32_t sessionID);
  void endAllCalls();

  void getSDPs(uint32_t sessionID,
               std::shared_ptr<SDPMessageInfo>& localSDP,
               std::shared_ptr<SDPMessageInfo>& remoteSDP);

  void failedToSendMessage();
  void updateAddress();


signals:

  void transportRequest(uint32_t sessionID, SIPRequest &request, QVariant& content);
  void transportResponse(uint32_t sessionID, SIPResponse &response, QVariant& content);

public slots:

  // when sip connection has received a request/response it is handled here.
  bool identifySession(SIPRequest request, QHostAddress localAddress, uint32_t& out_sessionID);
  void processSIPRequest(SIPRequest request, QHostAddress localAddress, QVariant& content, uint32_t sessionID);
  void processSIPResponse(SIPResponse response, QHostAddress localAddress, QVariant& content);

private slots:

  void sendDialogRequest(uint32_t sessionID, RequestType type);
  void sendNonDialogRequest(SIP_URI& uri, RequestType type);

  void sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest);

private:

  enum CallConnectionType {PEERTOPEER, SIPSERVER, CONTACT};

  struct SIPDialogData
  {
    std::shared_ptr<SIPDialogState> state;
    // do not stop connection before responding to all requests
    std::shared_ptr<SIPServerTransaction> server;
    std::shared_ptr<SIPDialogClient> client;
    std::shared_ptr<SDPMessageInfo> localSdp_;
    std::shared_ptr<SDPMessageInfo> remoteSdp_;

    bool proxyConnection_;

    QHostAddress localAddress;

    CallConnectionType connectionType;
  };

  struct SIPRegistrationData
  {
    std::shared_ptr<SIPNonDialogClient> client;
    std::shared_ptr<SIPDialogState> state;

    uint32_t sessionID;
    QHostAddress localAddress;
  };


  std::shared_ptr<SIPRouting> createSIPRouting(QString remoteUsername,
                                               QString localAddress,
                                               QString remoteAddress, bool hostedSession);

  // returns whether we should continue with processing
  bool processSDP(uint32_t sessionID, QVariant &content, QHostAddress localAddress);

  void startPeerToPeerCall(uint32_t sessionID, QHostAddress localAddress, Contact& remote);
  uint32_t createDialogFromINVITE(QHostAddress localAddress,  std::shared_ptr<SIPMessageInfo> &invite);
  void createBaseDialog(uint32_t sessionID, QHostAddress &localAddress, std::shared_ptr<SIPDialogData>& dialog);
  void destroyDialog(std::shared_ptr<SIPDialogData> dialog);
  void removeDialog(uint32_t sessionID);

  bool areWeTheDestination();

  void registerTask();

  // This mutex makes sure that the dialog has been added to the dialogs_ list
  // before we are accessing it when receiving messages
  QMutex dialogMutex_;

  struct DialogRequest
  {
    uint32_t sessionID;
    RequestType type;
  };

  struct NonDialogRequest
  {
    SIP_URI request_uri;
    RequestType type;
  };

  QMutex pendingConnectionMutex_;

  // SessionID:s are used in this program to keep track of dialogs.
  // The CallID is not used because we could be calling ourselves
  // and using uint32_t is simpler than keeping track of tags.

  // TODO: sessionID should be called dialogID

  uint32_t nextSessionID_;
  QMap<uint32_t, std::shared_ptr<SIPDialogData>> dialogs_;
  std::map<QString, SIPRegistrationData> registrations_;


  QList<QString> directContactAddresses_;

  std::unique_ptr<SIPNonDialogClient> nonDialogClient_;

  GlobalSDPState sdp_;

  SIPTransactionUser* transactionUser_;
};
