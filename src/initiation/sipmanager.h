#pragma once

#include "initiation/transport/connectionserver.h"
#include "initiation/transaction/siptransactions.h"
#include "initiation/transaction/sipregistrations.h"
#include "initiation/negotiation/negotiation.h"

/* This is a manager class that manages interactions between all the different
 * components in Session Initiation Protocol (SIP). This class should implement
 * as little functionality as possible and focus on interactions.
 *
 * SIP consists of Transaction layer, Transport Layer and Transaction User (TU).
 * SIP uses Session Description Protocol (SDP) for negotiating the call session
 * parameters with peers.
 */

class SIPTransactionUser;


class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  // start listening to incoming SIP messages
  void init(SIPTransactionUser* callControl);
  void uninit();

  // REGISTER our information to SIP-registrar
  void bindToServer();

  // start a call with address. Returns generated sessionID
  uint32_t startCall(Contact& address);

  // TU wants something to happen.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);
  void endAllCalls();

  void negotiateReceivePorts();
  void negotiateConference();

  // Get the negotiated session media session descriptions. Use after call
  // has been negotiated.
  void getSDPs(uint32_t sessionID,
               std::shared_ptr<SDPMessageInfo>& localSDP,
               std::shared_ptr<SDPMessageInfo>& remoteSDP) const;

private slots:

  // somebody established a TCP connection with us
  void receiveTCPConnection(TCPConnection* con);
  // our outbound TCP connection was established.
  void connectionEstablished(quint32 transportID);

  // send the SIP message to a SIP User agent with transport layer. Attaches SDP message if needed.
  void transportRequest(uint32_t sessionID, SIPRequest &request);
  void transportResponse(uint32_t sessionID, SIPResponse &response);

  // send the SIP request to a proxy with transport layer.
  void transportToProxy(QString serverAddress, SIPRequest &request);

  // Process incoming SIP message. May create session if INVITE.
  void processSIPRequest(SIPRequest &request, QHostAddress localAddress,
                         QVariant& content, quint32 transportID);
  void processSIPResponse(SIPResponse &response, QVariant& content);

private:

  std::shared_ptr<SIPTransport> createSIPTransport();

  // Goes through our current connections and returns if we are already connected
  // to this address. Sets found transportID.
  bool isConnected(QString remoteAddress, quint32& outTransportID);

  // Helper functions for SDP management.

  // When sending an SDP offer
  bool SDPOfferToContent(QVariant &content, QHostAddress localAddress, uint32_t sessionID);
  // When receiving an SDP offer
  bool processOfferSDP(uint32_t sessionID, QVariant &content, QHostAddress localAddress);
  // When sending an SDP answer
  bool SDPAnswerToContent(QVariant &content, uint32_t sessionID);
  // When receiving an SDP answer
  bool processAnswerSDP(uint32_t sessionID, QVariant &content);

  ConnectionServer tcpServer_;
  uint16_t sipPort_;

  // SIP Transport layer
  QMap<quint32, std::shared_ptr<SIPTransport>> transports_;
  quint32 nextTransportID_; // the next free transportID to be allocated

  // SIP Transactions layer
  SIPTransactions transactions_;

  SIPRegistrations registrations_;

  // mapping of which sessionID uses which TransportID
  std::map<uint32_t, quint32> sessionToTransportID_;

  // mapping of which SIP proxy uses which TransportID
  std::map<QString, quint32> serverToTransportID_;

  // if we want to do something, but the TCP connection has not yet been established
  struct WaitingStart
  {
    uint32_t sessionID;
    Contact contact;
  };
  std::map<quint32, WaitingStart> waitingToStart_; // INVITE
  std::map<quint32, QString> waitingToBind_; // REGISTER

  // Negotiation with SDP and ICE
  Negotiation negotiation_;


  enum NegotiateState {
    INDIVIDUAL,
    RECEIVE_PORTS,
    WHOLE_CONFERENCE
  };

  NegotiateState state_;
};
