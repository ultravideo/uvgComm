#pragma once

#include "initiation/transport/connectionserver.h"
#include "initiation/transport/siptransport.h"

#include "initiation/transaction/sipregistration.h"
#include "initiation/negotiation/negotiation.h"

#include "initiation/transaction/sipserver.h"
#include "initiation/transaction/sipclient.h"
#include "initiation/transaction/sipdialogstate.h"
#include "initiation/transaction/sipsinglecall.h"

/* This is a manager class that manages interactions between different
 * components in Session Initiation Protocol (SIP). This class should implement
 * as little functionality as possible.
 *
 * SIP consists of Transaction layer, Transport Layer and Transaction User (TU).
 * SIP uses Session Description Protocol (SDP) for negotiating the call session
 * parameters with peers.
 */

class SIPTransactionUser;
class StatisticsInterface;

// only for usage within this class
struct SIPDialog
{
  SIPSingleCall call;

  SIPDialogState state;
  std::shared_ptr<SIPClient> client;
  std::shared_ptr<SIPServer> server;

  // Handles SDP and ICE functionality
  std::shared_ptr<Negotiation> negotiation;
};


class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  // start listening to incoming SIP messages
  void init(SIPTransactionUser* callControl, StatisticsInterface *stats,
            ServerStatusView *statusView);
  void uninit();

  void updateSettings();

  // start a call with address. Returns generated sessionID
  uint32_t startCall(NameAddr &address);

  // TU wants something to happen.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);
  void endAllCalls();

  // Get the negotiated session media session descriptions. Use after call
  // has been negotiated.
  void getSDPs(uint32_t sessionID,
               std::shared_ptr<SDPMessageInfo>& localSDP,
               std::shared_ptr<SDPMessageInfo>& remoteSDP) const;

signals:
  void nominationSucceeded(quint32 sessionID);
  void nominationFailed(quint32 sessionID);

private slots:

  // somebody established a TCP connection with us
  void receiveTCPConnection(TCPConnection* con);
  // our outbound TCP connection was established.
  void connectionEstablished(QString localAddress, QString remoteAddress);

  // send the SIP message to a SIP User agent with transport layer. Attaches SDP message if needed.
  void transportRequest(uint32_t sessionID, SIPRequest &request);
  void transportResponse(uint32_t sessionID, SIPResponse &response);

  // send the SIP request to a proxy with transport layer.
  void transportToProxy(QString serverAddress, SIPRequest &request);

  // Process incoming SIP message. May create session if it's an INVITE.
  void processSIPRequest(SIPRequest &request,
                         QVariant& content, QString localAddress);
  void processSIPResponse(SIPResponse &response, QVariant& content, QString localAddress);

  void createDialog(uint32_t sessionID);
  void removeDialog(uint32_t sessionID);

private:

  std::shared_ptr<SIPDialog> getDialog(uint32_t sessionID) const;

  uint32_t createDialogFromINVITE(QString localAddress,
                                  SIPRequest &invite);

  bool haveWeRegistered();

  // returns true if the identification was successful
  bool identifySession(SIPRequest &request, QString localAddress,
                       uint32_t& out_sessionID);

  bool identifySession(SIPResponse &response,
                       uint32_t& out_sessionID);

  // start a call with address. Returns generated sessionID
  void sendINVITE(NameAddr &address, QString localAddress,
                 uint32_t sessionID, bool registered);

  // reserve sessionID for a future call
  uint32_t reserveSessionID();

  // REGISTER our information to SIP-registrar
  void bindToServer();

  // helper function which handles all steps related to creation of new transport
  std::shared_ptr<SIPTransport> createSIPTransport(QString address);

  // Goes through our current connections and returns if we are already connected
  // to this address.
  bool isConnected(QString remoteAddress);

  // Helper functions for SDP management.

  ConnectionServer tcpServer_;
  uint16_t sipPort_;

  // SIP Transport layer
  // Key is remote address
  QMap<QString, std::shared_ptr<SIPTransport>> transports_;

  std::map<QString, std::shared_ptr<SIPRegistration>> registrations_;

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

  // This mutex makes sure that the dialog has been added to the dialogs_ list
  // before we are accessing it when receiving messages
  std::map<uint32_t, std::shared_ptr<SIPDialog>> dialogs_;

  SIPTransactionUser* transactionUser_;
  ServerStatusView *statusView_;
};
