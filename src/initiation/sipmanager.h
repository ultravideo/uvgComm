#pragma once

#include "initiation/transport/connectionserver.h"

#include "initiation/transaction/sipcallbacks.h"

#include "initiation/sipmessageflow.h"

#include <QObject>
#include <QTimer>
#include <map>

#include <functional>
#include <queue>

class SIPServer;
class SIPClient;
class SDPNegotiation;

// The components specific to one dialog
struct DialogInstance
{
  SIPMessageFlow pipe;
  // state is used to find out whether message belongs to this dialog
  std::shared_ptr<SIPDialogState> state;
  std::shared_ptr<SIPServer> server; // for identifying cancel and sending responses
  std::shared_ptr<SIPClient> client; // for sending requests
  std::shared_ptr<SDPNegotiation> sdp; // for sending requests

  std::shared_ptr<SIPCallbacks> callbacks;
};

// Components specific to one registration
struct RegistrationInstance
{
  SIPMessageFlow pipe;
  std::shared_ptr<SIPDialogState> state;
  std::shared_ptr<SIPClient> client;

  std::shared_ptr<SIPCallbacks> callbacks;

  QString serverAddress;
  QTimer retryTimer;
};

// Components specific to one transport connection
struct TransportInstance
{
  std::shared_ptr<TCPConnection> connection;
  SIPMessageFlow pipe;
};

class StatisticsInterface;
class NetworkCandidates;
class SDPMeshConference;
struct SDPMessageInfo;

/* This is a manager class that manages interactions between different
 * components in Session Initiation Protocol (SIP). This class should implement
 * as little functionality as possible.
 *
 * SIP consists of Transaction layer, Transport Layer and Transaction User (TU).
 * SIP uses Session Description Protocol (SDP) for negotiating the call session
 * parameters with peers.
 */

enum SIPConnectionType
{
  SIP_UDP,
  SIP_TCP,
  SIP_TLS
};


struct SIPConfig
{
  bool sendRegister;
  QString sipServerAddress;
  uint16_t sipServerPort;

  bool conferencing;

  uint16_t mediaPort;

  bool ice;
  bool privateAddresses;

  bool stun;
  QString stunServerAddress;
  uint16_t stunServerPort;
};


class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  std::shared_ptr<SDPMessageInfo> generateSDP(QString username,
                                              int audioStreams, int videoStreams,
                                              QList<QString> dynamicAudioSubtypes = {},
                                              QList<QString> dynamicVideoSubtypes = {},
                                              QList<uint8_t> staticAudioPayloadTypes = {},
                                              QList<uint8_t> staticVideoPayloadTypes = {});

  void setSDP(std::shared_ptr<SDPMessageInfo> sdp);

  // start listening to incoming SIP messages
  void init(const SIPConfig& config, StatisticsInterface *stats);
  void uninit();

  bool listenToAny(SIPConnectionType type, uint16_t port);

  // returns whether the connection is ready
  bool connect(SIPConnectionType type, QString address, uint16_t port);
  void disconnect(QString remoteAddress); // TODO

  void bindingAtRegistrar(QString serverAddress);
  void removeBinding(QString serverAddress);

  uint32_t reserveSessionID();

  /* Creates all the necessary structures needed for sending Dialog Requests
   * and returns the sessionID of newly created Dialog. Does not send requests.
 */
  void createDialog(uint32_t sessionID, NameAddr &remote, QString remoteAddress);

  void sendINVITE(uint32_t sessionID);

  // TU wants something to happen.
  void respondRingingToINVITE(uint32_t sessionID);
  void respondOkToINVITE(uint32_t sessionID);
  void respondDeclineToINVITE(uint32_t sessionID);
  bool cancelCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);
  void endAllCalls();

  void installSIPRequestCallback(
      std::function<void(uint32_t sessionID, SIPRequest& request, QVariant& content)> callback);
  void installSIPResponseCallback(
      std::function<void(uint32_t sessionID, SIPResponse& response, QVariant& content)> callback);

  void installSIPRequestCallback(
      std::function<void(QString address, SIPRequest& request, QVariant& content)> callback);
  void installSIPResponseCallback(
      std::function<void(QString address, SIPResponse& response, QVariant& content)> callback);

  void clearCallbacks();

  // configure behavior
  void setConfig(const SIPConfig& config);
  SIPConfig getConfig();

  // TODO: These should be SDP connection details including type (all fields in SDP c= field)
  bool getSTUNBinding(uint32_t sessionID,
                      std::pair<QHostAddress, uint16_t> &inStunAddress,
                      std::pair<QHostAddress, uint16_t> &outStunBinding);

signals:

  void connectionFormed(QString address);

  void finalLocalSDP(const quint32 sessionID,
                     const std::shared_ptr<SDPMessageInfo> local);

  void connectionEstablished(QString localAddress, QString remoteAddress);

private slots:

  // somebody established a TCP connection with us
  void receiveTCPConnection(std::shared_ptr<TCPConnection> con);
  // our outbound TCP connection was established.

  // send the SIP message to a SIP User agent with transport layer. Attaches SDP message if needed.
  void transportRequest(SIPRequest &request, QVariant& content);
  void transportResponse(SIPResponse &response, QVariant& content);

  // Process incoming SIP message. May create session if it's an INVITE.
  void processSIPRequest(SIPRequest &request, QVariant& content,
                         SIPResponseStatus generatedResponse);
  void processSIPResponse(SIPResponse &response, QVariant& content,
                          bool retryRequest);

  void delayedMessage();

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

  // helper function which handles all steps related to creation of new transport
  void createSIPTransport(QString remoteAddress,
                          std::shared_ptr<TCPConnection> connection);

  void createRegistration(NameAddr &addressRecord);

  void createDialog(uint32_t sessionID, NameAddr &local,
                    NameAddr &remote, QString localAddress, bool ourDialog);
  void removeDialog(uint32_t sessionID);

  // Goes through our current connections and returns if we are already connected
  // to this address.
  bool isConnected(QString remoteAddress);

  void refreshDelayTimer();

  // If registered, we use the connection address in URI instead of our
  // server URI from settings.
  NameAddr localInfo(bool registered, QString connectionAddress);

  // get all values from settings.
  NameAddr localInfo();

  void re_INVITE_all();

  std::shared_ptr<TCPConnection> createConnection(SIPConnectionType type, QString address, uint16_t port);

  // Helper functions for SDP management.

  ConnectionServer tcpServer_;

  // SIP Transport layer
  // Key is remote address
  std::map<QString, std::shared_ptr<TransportInstance>> transports_;

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

  // these hold existing callbacks for incoming requests/responses
  std::vector<std::function<void(uint32_t sessionID,
                                 SIPRequest& request,
                                 QVariant& content)>> requestCallbacks_;

  std::vector<std::function<void(uint32_t sessionID,
                                 SIPResponse& response,
                                 QVariant& content)>> responseCallbacks_;

  // server versions
  std::vector<std::function<void(QString address,
                                 SIPRequest& request,
                                 QVariant& content)>> registrationsRequests_;

  std::vector<std::function<void(QString address,
                                 SIPResponse& response,
                                 QVariant& content)>> registrationResponses_;

  std::shared_ptr<SDPMessageInfo> ourSDP_;

  /* Used to avoid congestion when sending multiple messages
   * mutexing is not needed at this point since both adding and
   * processing are done by Qt main thread */
  QTimer delayTimer_;
  std::queue<uint32_t> dMessages_;

  std::shared_ptr<SDPMeshConference> sdpConf_;

  SIPConfig config_;

  QString cname_;
};
