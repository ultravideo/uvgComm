#include "sipmanager.h"

#include "initiation/negotiation/sdpmeshconference.h"
#include "initiation/negotiation/sdpnegotiation.h"
#include "initiation/negotiation/networkcandidates.h"
#include "initiation/negotiation/sdpice.h"

#include "initiation/transaction/sipserver.h"
#include "initiation/transaction/sipclient.h"
#include "initiation/transaction/sipdialogstate.h"
#include "initiation/transaction/sipallow.h"

#include "initiation/transport/siprouting.h"
#include "initiation/transport/tcpconnection.h"
#include "initiation/transport/sipauthentication.h"
#include "initiation/transport/siptransport.h"

#include "initiation/negotiation/sdpdefault.h"

#include "siphelper.h"

#include "common.h"
#include "global.h"
#include "settingskeys.h"
#include "logger.h"

#include <QNetworkProxy>

#include <QSettings>

#include <chrono>
#include <thread>

const uint32_t FIRSTSESSIONID = 1;
const int REGISTER_SEND_PERIOD = (REGISTER_INTERVAL - 5)*1000;

const int MIN_RANDOM_DELAY_MS = 25;
const int MAX_RANDOM_DELAY_MS = 75;

// default for SIP, use 5061 for tls encrypted
const uint16_t SIP_PORT = 5060;

const MeshType CONFERENCE_MODE = MESH_WITHOUT_RTP_MULTIPLEXING;

SIPManager::SIPManager():
  tcpServer_(),
  transports_(),
  nextSessionID_(FIRSTSESSIONID),
  dialogs_(),
  ourSDP_(nullptr),
  delayTimer_(),
  sdpConf_(std::shared_ptr<SDPMeshConference>(new SDPMeshConference()))
{
  delayTimer_.setSingleShot(true);
  QObject::connect(&delayTimer_, &QTimer::timeout,
                   this, &SIPManager::delayedMessage);

  if (settingEnabled(SettingsKey::sipP2PConferencing))
  {
    sdpConf_->setConferenceMode(CONFERENCE_MODE);
  }
}


std::shared_ptr<SDPMessageInfo> SIPManager::generateSDP(QString username,
                                                        int audioStreams, int videoStreams,
                                                        QList<QString> dynamicAudioSubtypes,
                                                        QList<QString> dynamicVideoSubtypes,
                                                        QList<uint8_t> staticAudioPayloadTypes,
                                                        QList<uint8_t> staticVideoPayloadTypes)
{
  if (audioStreams > 0 && dynamicAudioSubtypes.empty() && staticAudioPayloadTypes.empty())
  {
    return nullptr;
  }

  if (videoStreams > 0 && dynamicVideoSubtypes.empty() && staticVideoPayloadTypes.empty())
  {
    return nullptr;
  }

  return generateDefaultSDP(username, "", audioStreams, videoStreams,
                            dynamicAudioSubtypes, dynamicVideoSubtypes,
                            staticAudioPayloadTypes, staticVideoPayloadTypes);
}


void SIPManager::setSDP(std::shared_ptr<SDPMessageInfo> sdp)
{
  ourSDP_ = sdp;

  for (auto& dialog : dialogs_)
  {
    if (dialog.second != nullptr)
    {
      dialog.second->sdp->setBaseSDP(sdp);
    }
  }
}


void SIPManager::refreshDelayTimer()
{
  if (!dMessages_.empty() && !delayTimer_.isActive())
  {
    Logger::getLogger()->printNormal(this, "Starting timer for delayed messages",
                                     "Count", QString::number(dMessages_.size()));
    int delay = rand()%(MAX_RANDOM_DELAY_MS - MIN_RANDOM_DELAY_MS) + MIN_RANDOM_DELAY_MS;
    delayTimer_.start(delay);
  }
}


// start listening to incoming
void SIPManager::init(StatisticsInterface *stats)
{
  stats_ = stats;

  nCandidates_ = std::shared_ptr<NetworkCandidates> (new NetworkCandidates);
  nCandidates_->init();
}


void SIPManager::uninit()
{
  Logger::getLogger()->printNormal(this, "Uninit SIP Manager");

  for (auto& registration : registrations_)
  {
    if (registration.second->client->registrationActive())
    {
      registration.second->client->sendREGISTER(0);
    }

    // TODO: wait for ok from server before continuing

    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    registration.second->pipe.uninit();
  }

  registrations_.clear();

  for(auto& transport : transports_)
  {
    if(transport.second != nullptr)
    {
      if (transport.second->connection != nullptr)
      {

        transport.second->connection->exit(0); // stops qthread
        transport.second->connection->stopConnection(); // exits run loop
        while(transport.second->connection->isRunning())
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        transport.second->connection.reset();
      }
      transport.second->pipe.uninit();
      transport.second.reset();
    }
  }

  transports_.clear();

  for(auto& dialog : dialogs_)
  {
    dialog.second->pipe.uninit();
  }

  dialogs_.clear();
  sdpConf_->uninit();
  nextSessionID_ = FIRSTSESSIONID;
}


bool SIPManager::listenToAny(SIPConnectionType type, uint16_t port)
{
  if (type == SIP_TCP)
  {
    QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                     this, &SIPManager::receiveTCPConnection);

    tcpServer_.setProxy(QNetworkProxy::NoProxy);

    // listen to everything
    Logger::getLogger()->printNormal(this, "Listening to SIP TCP connections",
                                     "Port", QString::number(port));

    return tcpServer_.listen(QHostAddress::Any, port);
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Listening to SIP TCP connections",
                                           "Port", QString::number(port));
  }

  return false;
}


bool SIPManager::connect(SIPConnectionType type, QString address, uint16_t port)
{
  if (transports_.find(address) == transports_.end())
  {
    std::shared_ptr<TCPConnection> connection =
        std::shared_ptr<TCPConnection>(new TCPConnection());
    createSIPTransport(address, createConnection(type, address, port));
  }
  else
  {
    return true;
  }

  return false;
}


// reserve sessionID for a future call
uint32_t SIPManager::reserveSessionID()
{
  ++nextSessionID_;
  return nextSessionID_ - 1;
}


void SIPManager::updateCallSettings()
{
  nCandidates_->init();
  if (settingEnabled(SettingsKey::sipP2PConferencing))
  {
    sdpConf_->setConferenceMode(CONFERENCE_MODE);
  }
  else
  {
    sdpConf_->setConferenceMode(MESH_NO_CONFERENCE);
  }
}


void SIPManager::bindingAtRegistrar(QString serverAddress)
{
  if (transports_.find(serverAddress) == transports_.end())
  {
    Logger::getLogger()->printError(this, "No connection found when binding");
    return;
  }

  if (registrations_.find(serverAddress) == registrations_.end())
  {
    NameAddr local = localInfo();
    createRegistration(local);

    getRegistration(serverAddress)->client->sendREGISTER(REGISTER_INTERVAL);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Registration already exists, not doing anything");
  }
}


void SIPManager::createDialog(uint32_t sessionID, NameAddr &remote, QString remoteAddress)
{
  QString connectionAddress = remote.uri.hostport.host;
  QString ourProxyAddress = haveWeRegistered();

  // check if this should be a peer-to-peer connection or if we should use proxy
  bool useOurProxy = shouldUseProxy(remote.uri.hostport.host);

  if (useOurProxy)
  {
    Logger::getLogger()->printNormal(this, "Using our proxy address", "Address", ourProxyAddress);
    connectionAddress = ourProxyAddress;
  }

  if (!isConnected(connectionAddress))
  {
    Logger::getLogger()->printError(this, "Creating a dialog for non-active connection");
    return;
  }

  QString localAddress = getTransport(remoteAddress)->connection->localAddress();

  // get correct local URI
  NameAddr local = localInfo(useOurProxy, localAddress);

  createDialog(sessionID, local, remote, localAddress, true);
}


void SIPManager::sendINVITE(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  Logger::getLogger()->printNormal(this, "Sending Re-INVITE", {"SessionID"},
                                   {QString::number(sessionID)});

  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
  dialog->client->sendINVITE(INVITE_TIMEOUT);
}


void SIPManager::respondRingingToINVITE(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  Logger::getLogger()->printNormal(this, "Sending that we are RINGING", {"SessionID"},
                                   {QString::number(sessionID)});

  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
  dialog->server->respond_INVITE_RINGING();
}


void SIPManager::respondOkToINVITE(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  Logger::getLogger()->printNormal(this, "Accepting call", {"SessionID"},
                                   {QString::number(sessionID)});

  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
  dialog->server->respond_INVITE_OK();
}


void SIPManager::respondDeclineToINVITE(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);

  dialog->server->respond_INVITE_DECLINE();

  removeDialog(sessionID);
}


bool SIPManager::cancelCall(uint32_t sessionID)
{
  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
    dialog->client->sendCANCEL();
    //removeDialog(sessionID);
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Tried to remove a non-existing dialog");
    return false;
  }
  return true;
}


void SIPManager::endCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
  dialog->client->sendBYE();

  // TODO: Do this when receiving BYE OK
  removeDialog(sessionID);
}


void SIPManager::endAllCalls()
{
  for(auto& dialog : dialogs_)
  {
    if(dialog.second != nullptr)
    {
      dialog.second->client->sendBYE();

      // the call structures are removed when we receive an OK
    }
  }

  nextSessionID_ = FIRSTSESSIONID;
}


void SIPManager::receiveTCPConnection(std::shared_ptr<TCPConnection> con)
{
  Logger::getLogger()->printNormal(this, "Received a TCP connection. Initializing dialog.");
  Q_ASSERT(con);

  if (!con->waitUntilConnected())
  {
    Logger::getLogger()->printWarning(this, "Did not manage to connect incoming connection!");
    return;
  }

  // this informs us when the connection has been established
  QObject::connect(con.get(), &TCPConnection::socketConnected,
                   this,      &SIPManager::connectionEstablished);

  createSIPTransport(con->remoteAddress(), con);
}


void SIPManager::transportRequest(SIPRequest &request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Initiate sending of a dialog request");

  std::shared_ptr<TransportInstance> transport =
      getTransport(request.message->to.address.uri.hostport.host);

  if (transport != nullptr)
  {
    transport->pipe.processOutgoingRequest(request, content);
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR,  metaObject()->className(),
               "Tried to send request when we are not connected to request-URI");
  }
}


void SIPManager::transportResponse(SIPResponse &response, QVariant& content)
{
  std::shared_ptr<TransportInstance> transport =
      getTransport(response.message->from.address.uri.hostport.host);

  if (transport != nullptr)
  {
    Logger::getLogger()->printNormal(this, "Found correct transport. Giving response to transport layer.");

    // send the request with or without SDP
    transport->pipe.processOutgoingResponse(response, content);
  }
  else {
    Logger::getLogger()->printDebug(DEBUG_ERROR, metaObject()->className(),
                                   "Tried to send response with invalid.");
  }
}


void SIPManager::processSIPRequest(SIPRequest& request, QVariant& content,
                                   SIPResponseStatus generatedResponse)
{
  Logger::getLogger()->printNormal(this, "Processing incoming request");

  uint32_t sessionID = 0;

  // sets the sessionID if session exists or creates a new session if INVITE
  // returns true if either was successful.
  if (!identifySession(request, sessionID))
  {
    Logger::getLogger()->printNormal(this, "No existing dialog found.");

    if(request.method == SIP_INVITE)
    {
      QString localAddress = "";
      QString proxyAddress = haveWeRegistered();

      // try to determine our local address in case we don't have registrations
      // and need it fo our local URI.
      if (transports_.size() == 1)
      {
        // if we have one address, it must be it
        localAddress = transports_.begin()->second->connection->localAddress();
      }
      else if (!request.message->vias.empty() &&
               transports_.find(request.message->vias.last().sentBy) != transports_.end())
      {
        // If the last via matches the local address. Via determines the last
        // place the request has been routed through so it should be the other
        // end of that connection.
        localAddress = transports_[request.message->vias.last().sentBy]->connection->localAddress();
      }
      else if (proxyAddress != "")
      {
        // use a regisration if we have one as a last resort
        localAddress = transports_[proxyAddress]->connection->localAddress();
      }
      else
      {
        Logger::getLogger()->printError(this, "Couldn't determine our local URI!");
        return;
      }

      Logger::getLogger()->printNormal(this, "Someone is trying to start a SIP dialog with us!");
      sessionID = reserveSessionID();

      // if they used our local address in to, we use that also, otherwise we use our binding
      NameAddr local = localInfo(localAddress != request.message->to.address.uri.hostport.host,
                                 localAddress);

      createDialog(sessionID, local, request.message->from.address, localAddress, false);
    }
    else if (request.method == SIP_CANCEL && identifyCANCELSession(request, sessionID))
    {
      Logger::getLogger()->printNormal(this, "They are cancelling their request");
      // the processing of CANCEL is handled in same way as other requests, it is just
      // identified differently
    }
    else
    {
      Logger::getLogger()->printUnimplemented(this, "Out of Dialog request");
      return;
    }
  }

  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  Logger::getLogger()->printNormal(this, "Starting to process received SIP Request.");

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<DialogInstance> foundDialog = getDialog(sessionID);

  foundDialog->pipe.processIncomingRequest(request, content, generatedResponse);

  if(foundDialog->server->shouldBeDestroyed())
  {
    Logger::getLogger()->printNormal(this, "Ending session as a results of request.");
    removeDialog(sessionID);
  }

  Logger::getLogger()->printNormal(this, "Finished processing request.",
    {"SessionID"}, {QString::number(sessionID)});
}


void SIPManager::processSIPResponse(SIPResponse &response, QVariant& content,
                                    bool retryRequest)
{
  Logger::getLogger()->printNormal(this, "Processing incoming response");

  if (response.message == nullptr)
  {
    Logger::getLogger()->printProgramError(this, "Process response got a message without header");
    return;
  }

  for (auto& registration : registrations_)
  {
    if(registration.second->state->correctResponseDialog(response.message->callID,
                                                         response.message->to.tagParameter,
                                                         response.message->from.tagParameter))
    {
      Logger::getLogger()->printNormal(this, "Got a response to server message!");
      registration.second->pipe.processIncomingResponse(response, content, retryRequest);
      return;
    }
  }

  uint32_t sessionID = 0;

  if(!identifySession(response, sessionID) || sessionID == 0)
  {
    Logger::getLogger()->printDebug(DEBUG_PEER_ERROR, this,
               "Could not identify response session");
    return;
  }

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<DialogInstance> foundDialog = getDialog(sessionID);

  foundDialog->pipe.processIncomingResponse(response, content, retryRequest);

  if (foundDialog->client->shouldBeDestroyed())
  {
    Logger::getLogger()->printNormal(this, "Ending session as a results of response.");
    removeDialog(sessionID);
  }

  Logger::getLogger()->printNormal(this, "Response processing finished",
      {"SessionID"}, {QString::number(sessionID)});
}


bool SIPManager::isConnected(QString remoteAddress)
{
  return transports_.find(remoteAddress) != transports_.end();
}


bool SIPManager::identifySession(SIPRequest& request,
                                 uint32_t& out_sessionID)
{
  Logger::getLogger()->printNormal(this, "Starting to process identifying SIP Request dialog.");

  out_sessionID = 0;

  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if (i->second != nullptr &&
        i->second->state->correctRequestDialog(request.message->callID,
                                               request.message->to.tagParameter,
                                               request.message->from.tagParameter))
    {
      Logger::getLogger()->printNormal(this, "Found matching dialog for incoming request.");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


bool SIPManager::identifySession(SIPResponse &response, uint32_t& out_sessionID)
{
  Logger::getLogger()->printNormal(this, "Starting to process identifying SIP response dialog.");

  out_sessionID = 0;
  // find the dialog which corresponds to the callID and tags received in response
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if (i->second != nullptr &&
        i->second->state->correctResponseDialog(response.message->callID,
                                                response.message->to.tagParameter,
                                                response.message->from.tagParameter))
    {
      Logger::getLogger()->printNormal(this, "Found matching dialog for incoming response");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


bool SIPManager::identifyCANCELSession(SIPRequest &request, uint32_t& out_sessionID)
{
  out_sessionID = 0;
  // find the request which is being cancelled
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if (i->second != nullptr &&
        i->second->server->doesCANCELMatchRequest(request))
    {
      Logger::getLogger()->printNormal(this, "Found matching request for cancellation");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


std::shared_ptr<DialogInstance> SIPManager::getDialog(uint32_t sessionID) const
{
  std::shared_ptr<DialogInstance> foundDialog = nullptr;
  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    foundDialog = dialogs_.at(sessionID);
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Could not find dialog",
                                           "SessionID", QString::number(sessionID));
  }

  return foundDialog;
}


std::shared_ptr<RegistrationInstance> SIPManager::getRegistration(QString& address) const
{
  std::shared_ptr<RegistrationInstance> foundRegistration = nullptr;
  if (registrations_.find(address) != registrations_.end())
  {
    foundRegistration = registrations_.at(address);
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Could not find registration",
                      "Address", address);
  }

  return foundRegistration;
}


std::shared_ptr<TransportInstance> SIPManager::getTransport(QString& address) const
{
  std::shared_ptr<TransportInstance> foundTransport = nullptr;
  if (transports_.find(address) != transports_.end())
  {
    foundTransport = transports_.at(address);
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Could not find transport",
                      {"Address", "Number of transports"}, {address, QString::number(transports_.size())});

    if (transports_.size() == 1)
    {
      Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Only existing transport address",
                        {"Address"}, {transports_.begin()->first});
    }

    QHostAddress differentAddress(address);

    if (differentAddress.protocol() == QAbstractSocket::IPv4Protocol)
    {
      differentAddress = QHostAddress(differentAddress.toIPv6Address());
    }
    else
    {
      differentAddress = QHostAddress(differentAddress.toIPv4Address());
    }

    if (transports_.find(differentAddress.toString()) != transports_.end())
    {
      Logger::getLogger()->printNormal(this, "Different IP version matches!");
      foundTransport = transports_.at(differentAddress.toString());
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_ERROR, this, "Different IP version was not successful either",
                        {"Different IP Address", }, {differentAddress.toString()});
    }
  }

  return foundTransport;
}


void SIPManager::createSIPTransport(QString remoteAddress, std::shared_ptr<TCPConnection> connection)
{
  /* SIP is divided to transport and transaction layers. Here we construct the transport
   * layer for one connection (either to proxy or to peer) which is used by one or more
   * transaction layer dialogs and/or registrations.
   */

  Q_ASSERT(stats_);

  if (transports_.find(remoteAddress) == transports_.end())
  {
    Logger::getLogger()->printNormal(this, "Creating SIP transport.", "Previous transports",
                QString::number(transports_.size()));

    std::shared_ptr<TransportInstance> instance =
        std::shared_ptr<TransportInstance> (new TransportInstance);
    transports_[remoteAddress] = instance;

    instance->connection = connection;

    std::shared_ptr<SIPTransport> transport =
        std::shared_ptr<SIPTransport>(new SIPTransport(stats_));

    std::shared_ptr<SIPRouting> routing =
        std::shared_ptr<SIPRouting> (new SIPRouting(instance->connection));

    std::shared_ptr<SIPAuthentication> authentication =
        std::shared_ptr<SIPAuthentication> (new SIPAuthentication());

    // get those network messages to transport
    QObject::connect(connection.get(), &TCPConnection::messageAvailable,
                     transport.get(),  &SIPTransport::networkPackage);

    // get those network messages to transport
    QObject::connect(transport.get(),             &SIPTransport::sendMessage,
                     instance->connection.get(),  &TCPConnection::sendPacket);

    // remember that the processors are always added from outgoing to incoming
    instance->pipe.addProcessor(transport);
    instance->pipe.addProcessor(authentication);
    instance->pipe.addProcessor(routing);

    QObject::connect(&instance->pipe, &SIPMessageFlow::incomingRequest,
                     this,            &SIPManager::processSIPRequest);

    QObject::connect(&instance->pipe, &SIPMessageFlow::incomingResponse,
                     this,            &SIPManager::processSIPResponse);
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Not creating SIP transport since it already exists");
  }

  // In case the connection has already received a message,
  // we announce that we are ready to process them
  connection->allowReceiving();
}


std::shared_ptr<TCPConnection> SIPManager::createConnection(SIPConnectionType type, QString address, uint16_t port)
{
  if (type == SIP_TCP)
  {
    std::shared_ptr<TCPConnection> connection =
        std::shared_ptr<TCPConnection>(new TCPConnection());

    // this informs us when the connection has been established
    QObject::connect(connection.get(), &TCPConnection::socketConnected,
                     this,             &SIPManager::connectionEstablished);

    connection->establishConnection(address, port);
    return connection;
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Non-TCP connection in transport creation");
  }
  return nullptr;
}


void SIPManager::createRegistration(NameAddr& addressRecord)
{
  /* SIP is divided to transport and transaction layers. Here we construct the transaction
   * part of connection with the server (mostly for sending REGISTER requests).
   */

  if (registrations_.find(addressRecord.uri.hostport.host) == registrations_.end())
  {
    std::shared_ptr<RegistrationInstance> registration =
        std::shared_ptr<RegistrationInstance> (new RegistrationInstance);
    registrations_[addressRecord.uri.hostport.host] = registration;

    //registration->registration.init(statusView_);

    registration->serverAddress = addressRecord.uri.hostport.host;

    registration->state = std::shared_ptr<SIPDialogState> (new SIPDialogState);
    registration->callbacks = std::shared_ptr<SIPCallbacks> (new SIPCallbacks(addressRecord.uri.hostport.host,
                                                                              registrationsRequests_,
                                                                              registrationResponses_));

    SIP_URI serverUri = {DEFAULT_SIP_TYPE, {"", ""},
                         {addressRecord.uri.hostport.host, 0}, {}, {}};
    registration->state->createServerConnection(addressRecord, serverUri);


    registration->client = std::shared_ptr<SIPClient> (new SIPClient);
    //std::shared_ptr<SIPServer> server = std::shared_ptr<SIPServer> (new SIPServer);

    // Add all components to the pipe.
    registration->pipe.addProcessor(registration->state);
    registration->pipe.addProcessor(registration->client);
    //registration->pipe.addProcessor(server);


    // Connect the pipe to registration and transmission functions.
    registration->callbacks->connectOutgoingProcessor(registration->pipe);
    registration->pipe.connectIncomingProcessor(*registration->callbacks);

    QObject::connect(&registration->pipe, &SIPMessageFlow::outgoingRequest,
                     this, &SIPManager::transportRequest);

    QObject::connect(&registration->pipe, &SIPMessageFlow::outgoingResponse,
                     this, &SIPManager::transportResponse);

    QObject::connect(&registration->retryTimer,  &QTimer::timeout,
                     registration->client.get(), &SIPClient::refreshRegistration);

    registration->retryTimer.setInterval(REGISTER_SEND_PERIOD);
    registration->retryTimer.setSingleShot(false);

    registration->retryTimer.start();
  }
}


void SIPManager::createDialog(uint32_t sessionID, NameAddr &local,
                              NameAddr &remote, QString localAddress, bool ourDialog)
{
  /* SIP is divided to transport and transaction layers. Here we construct the transaction
   * part of connection with one peer.
   */

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    Logger::getLogger()->printWarning(this, "Previous dialog existed with same sessionID as new one!");
    removeDialog(sessionID);
  }

  // Create all the components of the flow. Currently the pipe components are
  // only stored in pipe.
  std::shared_ptr<DialogInstance> dialog = std::shared_ptr<DialogInstance> (new DialogInstance);
  dialogs_[sessionID] = dialog;

  if (ourSDP_ == nullptr)
  {
    ourSDP_ = generateDefaultSDP(getLocalUsername(), "",  1, 1, {"opus"}, {"H265"}, {0}, {});
  }

  std::shared_ptr<SDPMessageInfo> sdp = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  *sdp = *ourSDP_;

  dialog->sdp = std::shared_ptr<SDPNegotiation> (new SDPNegotiation(sessionID, localAddress, sdp, sdpConf_));
  std::shared_ptr<SDPICE> ice = std::shared_ptr<SDPICE> (new SDPICE(nCandidates_, sessionID));

  // we need a way to get our final SDP to the SIP user
  QObject::connect(ice.get(), &SDPICE::localSDPWithCandidates,
                   this, &SIPManager::finalLocalSDP);

  if (settingEnabled(SettingsKey::sipP2PConferencing) && CONFERENCE_MODE == MESH_WITH_RTP_MULTIPLEXING)
  {
    ice->limitMediaCandidates(ourSDP_->media.size());
  }

  dialog->client = std::shared_ptr<SIPClient> (new SIPClient);
  dialog->server = std::shared_ptr<SIPServer> (new SIPServer);
  dialog->callbacks = std::shared_ptr<SIPCallbacks> (new SIPCallbacks(sessionID,
                                                                      requestCallbacks_,
                                                                      responseCallbacks_));

  // Initiatiate all the components of the flow.
  dialog->state = std::shared_ptr<SIPDialogState> (new SIPDialogState);
  dialog->state->init(local, remote, ourDialog);

  // Add all components to the pipe.
  dialog->pipe.addProcessor(std::shared_ptr<SIPAllow>(new SIPAllow));
  dialog->pipe.addProcessor(dialog->state);
  dialog->pipe.addProcessor(ice);
  dialog->pipe.addProcessor(dialog->sdp);
  dialog->pipe.addProcessor(dialog->client);
  dialog->pipe.addProcessor(dialog->server);

  // Connect the pipe to call and transmission functions.
  dialog->callbacks->connectOutgoingProcessor(dialog->pipe);
  dialog->pipe.connectIncomingProcessor(*dialog->callbacks);

  QObject::connect(&dialogs_[sessionID]->pipe, &SIPMessageFlow::outgoingRequest,
                   this, &SIPManager::transportRequest);

  QObject::connect(&dialogs_[sessionID]->pipe, &SIPMessageFlow::outgoingResponse,
                   this, &SIPManager::transportResponse);
}


void SIPManager::removeDialog(uint32_t sessionID)
{
  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    getDialog(sessionID)->pipe.uninit();
  }

  dialogs_.erase(dialogs_.find(sessionID));
  if (dialogs_.empty())
  {
    nextSessionID_ = FIRSTSESSIONID;
  }

  sdpConf_->removeRemoteSDP(sessionID);
}


void SIPManager::installSIPRequestCallback(std::function<void(uint32_t sessionID,
                                                              SIPRequest& request,
                                                              QVariant& content)> callback)
{
  requestCallbacks_.push_back(callback);

  for (auto& dialog: dialogs_)
  {
    if (dialog.second != nullptr)
    {
      dialog.second->callbacks->installSIPRequestCallback(callback);
    }
  }
}


void SIPManager::installSIPResponseCallback(std::function<void(uint32_t sessionID,
                                                               SIPResponse& response,
                                                               QVariant& content)> callback)
{
  responseCallbacks_.push_back(callback);

  for (auto& dialog: dialogs_)
  {
    if (dialog.second != nullptr)
    {
      dialog.second->callbacks->installSIPResponseCallback(callback);
    }
  }
}

void SIPManager::installSIPRequestCallback(
    std::function<void(QString address, SIPRequest& request, QVariant& content)> callback)
{
  registrationsRequests_.push_back(callback);

  for (auto& registration: registrations_)
  {
    if (registration.second != nullptr)
    {
      registration.second->callbacks->installSIPRequestCallback(callback);
    }
  }
}


void SIPManager::installSIPResponseCallback(
    std::function<void(QString address, SIPResponse& response, QVariant& content)> callback)
{
  registrationResponses_.push_back(callback);

  for (auto& registration: registrations_)
  {
    if (registration.second != nullptr)
    {
      registration.second->callbacks->installSIPResponseCallback(callback);
    }
  }
}


void SIPManager::clearCallbacks()
{
  requestCallbacks_.clear();
  responseCallbacks_.clear();

  for (auto& dialog: dialogs_)
  {
    if (dialog.second != nullptr)
    {
      dialog.second->callbacks->clearCallbacks();
    }
  }

  registrationsRequests_.clear();
  registrationResponses_.clear();

  for (auto& registration: registrations_)
  {
    if (registration.second != nullptr)
    {
      registration.second->callbacks->clearCallbacks();
    }
  }
}


QString SIPManager::haveWeRegistered()
{
  for (auto& registration : registrations_)
  {
    if (registration.second->client->registrationActive())
    {
      return registration.second->serverAddress;
    }
  }
  return "";
}


bool SIPManager::shouldUseProxy(QString remoteAddress)
{
  // The purpose of this function is to determine if we can use the proxy to
  // contact the remote AOR (SIP:username@proxy). We assume here that if we want to
  // call a loopback address, we just establish a direct connection

  QHostAddress remote = QHostAddress(remoteAddress);

  // if we have registered and the remote AOR is not loopback
  return !remote.isLoopback() && !isPrivateNetwork(remoteAddress.toStdString()) &&
      haveWeRegistered() != "";
}


NameAddr SIPManager::localInfo(bool registered, QString connectionAddress)
{
  NameAddr address = localInfo();
  // don't set server address if we have already set peer-to-peer address
  if (!registered)
  {
    address.uri.hostport.host = connectionAddress;
  }

  return address;
}


NameAddr SIPManager::localInfo()
{
  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  NameAddr local;

  local.realname = settings.value("local/Name").toString();
  local.uri.userinfo.user = getLocalUsername();

  // dont set server address if we have already set peer-to-peer address
  local.uri.hostport.host = settings.value("sip/ServerAddress").toString();

  local.uri.type = DEFAULT_SIP_TYPE;
  local.uri.hostport.port = 0; // port is added later if needed

  if(local.uri.userinfo.user.isEmpty())
  {
    local.uri.userinfo.user = "anonymous";
  }

  return local;
}


void SIPManager::delayedMessage()
{
  if (dMessages_.empty())
  {
    Logger::getLogger()->printProgramWarning(this, "No delayed messages in queue");
    return;
  }

  Logger::getLogger()->printNormal(this, "Sending delayed re-INVITE");

  uint32_t sessionID = dMessages_.front();
  dMessages_.pop();

  // so far only INVITE is supported, but others could easily be supported
  sendINVITE(sessionID);

  refreshDelayTimer();
}


void SIPManager::re_INVITE_all()
{
  Logger::getLogger()->printNormal(this, "Sending Re-INVITE through all our "
                                         "dialogs since our SDP has changed");

  for (auto& dialog : dialogs_)
  {
    if (dialog.second != nullptr)
    {
      // only send Re-INVITE for dialogs that have an active call ongoing
      if (dialog.second->state->isCallActive())
      {
        dMessages_.push(dialog.first);
      }
    }
  }

  refreshDelayTimer();
}
