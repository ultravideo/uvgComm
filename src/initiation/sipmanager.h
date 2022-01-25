#pragma once

#include "initiation/transport/connectionserver.h"
#include "initiation/transport/siptransport.h"

#include "initiation/transaction/sipregistration.h"
#include "initiation/transaction/sipsinglecall.h"


#include "initiation/sipmessageflow.h"

#include <QObject>

/* This is a manager class that manages interactions between different
 * components in Session Initiation Protocol (SIP). This class should implement
 * as little functionality as possible.
 *
 * SIP consists of Transaction layer, Transport Layer and Transaction User (TU).
 * SIP uses Session Description Protocol (SDP) for negotiating the call session
 * parameters with peers.
 */

class SIPServer;

// The components specific to one dialog
struct DialogInstance
{
  SIPMessageFlow pipe;
  // state is used to find out whether message belongs to this dialog
  std::shared_ptr<SIPDialogState> state;
  std::shared_ptr<SIPServer> server; // for identifying cancel
  SIPSingleCall call;
};

// Components specific to one registration
struct RegistrationInstance
{
  SIPMessageFlow pipe;
  std::shared_ptr<SIPDialogState> state;
  SIPRegistration registration;
};

// Components specific to one transport connection
struct TransportInstance
{
  std::shared_ptr<TCPConnection> connection;
  SIPMessageFlow pipe;
};

class SIPTransactionUser;
class StatisticsInterface;
class NetworkCandidates;



class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  // start listening to incoming SIP messages
  void init(SIPTransactionUser* callControl, StatisticsInterface *stats,
            ServerStatusView *statusView);
  void uninit();

  // start a call with address. Returns generated sessionID
  uint32_t startCall(NameAddr &remote);

  // TU wants something to happen.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);
  void endAllCalls();

public slots:
  void updateCallSettings();

signals:

  // ICE signals
  void nominationSucceeded(const quint32 sessionID,
                           const std::shared_ptr<SDPMessageInfo> local,
                           const std::shared_ptr<SDPMessageInfo> remote);
  void nominationFailed(quint32 sessionID);

private slots:

  // somebody established a TCP connection with us
  void receiveTCPConnection(std::shared_ptr<TCPConnection> con);
  // our outbound TCP connection was established.
  void connectionEstablished(QString localAddress, QString remoteAddress);

  // send the SIP message to a SIP User agent with transport layer. Attaches SDP message if needed.
  void transportRequest(SIPRequest &request, QVariant& content);
  void transportResponse(SIPResponse &response, QVariant& content);

  // Process incoming SIP message. May create session if it's an INVITE.
  void processSIPRequest(SIPRequest &request, QVariant& content);
  void processSIPResponse(SIPResponse &response, QVariant& content);

private:

  std::shared_ptr<DialogInstance> getDialog(uint32_t sessionID) const;
  std::shared_ptr<RegistrationInstance> getRegistration(QString& address) const;
  std::shared_ptr<TransportInstance> getTransport(QString& address) const;

  QString haveWeRegistered();

  bool shouldUseProxy(QString remoteAddress);

  // returns true if the identification was successful
  bool identifySession(SIPRequest &request,
                       uint32_t& out_sessionID);

  bool identifySession(SIPResponse &response,
                       uint32_t& out_sessionID);

  bool identifyCANCELSession(SIPRequest &request,
                             uint32_t& out_sessionID);

  // reserve sessionID for a future call
  uint32_t reserveSessionID();

  // REGISTER our information to SIP-registrar
  void bindToServer();

  // helper function which handles all steps related to creation of new transport
  void createSIPTransport(QString remoteAddress,
                          std::shared_ptr<TCPConnection> connection,
                          bool startConnection);

  void createRegistration(NameAddr &addressRecord);

  void createDialog(uint32_t sessionID, NameAddr &local,
                    NameAddr &remote, QString localAddress, bool ourDialog);
  void removeDialog(uint32_t sessionID);

  // Goes through our current connections and returns if we are already connected
  // to this address.
  bool isConnected(QString remoteAddress);

  // If registered, we use the connection address in URI instead of our
  // server URI from settings.
  NameAddr localInfo(bool registered, QString connectionAddress);

  // get all values from settings.
  NameAddr localInfo();

  // Helper functions for SDP management.

  ConnectionServer tcpServer_;
  uint16_t sipPort_;

  // SIP Transport layer
  // Key is remote address
  QMap<QString, std::shared_ptr<TransportInstance>> transports_;

  // if we want to do something, but the TCP connection has not yet been established
  struct WaitingStart
  {
    uint32_t sessionID;
    NameAddr contact;
  };

  std::map<QString, WaitingStart> waitingToStart_; // INVITE after connect
  QStringList waitingToBind_; // REGISTER after connect

  std::shared_ptr<NetworkCandidates> nCandidates_;

  StatisticsInterface *stats_;

  // SessionID:s are used in this program to keep track of dialogs.
  // The CallID is not used because we could be calling ourselves
  // and using uint32_t is simpler than keeping track of tags.

  // TODO: sessionID should be called dialogID

  uint32_t nextSessionID_;

  // key is sessionID
  std::map<uint32_t, std::shared_ptr<DialogInstance>> dialogs_;

  // key is the server address
  std::map<QString, std::shared_ptr<RegistrationInstance>> registrations_;

  SIPTransactionUser* transactionUser_;
  ServerStatusView *statusView_;
};
