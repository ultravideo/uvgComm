#include "sipmanager.h"

#include "initiation/negotiation/sdpnegotiation.h"
#include "initiation/negotiation/networkcandidates.h"
#include "initiation/negotiation/ice.h"
#include "initiation/transaction/sipserver.h"
#include "initiation/transaction/sipclient.h"
#include "initiation/transaction/sipdialogstate.h"

#include "initiation/transport/siprouting.h"
#include "initiation/transport/tcpconnection.h"
#include "initiation/transport/sipauthentication.h"

#include "initiation/negotiation/sdpdefault.h"

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

// default for SIP, use 5061 for tls encrypted
const uint16_t SIP_PORT = 5060;

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(SIP_PORT),
  transports_(),
  nextSessionID_(FIRSTSESSIONID),
  dialogs_(),
  ourSDP_(nullptr)
{}


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

  // TODO: Trigger re-INVITE for all dialogs
}


// start listening to incoming
void SIPManager::init(StatisticsInterface *stats)
{
  QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                   this, &SIPManager::receiveTCPConnection);

  stats_ = stats;

  tcpServer_.setProxy(QNetworkProxy::NoProxy);

  // listen to everything
  Logger::getLogger()->printNormal(this, "Listening to SIP TCP connections", 
                                   "Port", QString::number(sipPort_));

  if (!tcpServer_.listen(QHostAddress::Any, sipPort_))
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, this,
                                    "Failed to listen to socket. Is it reserved?");

    // TODO announce it to user!
  }

  int autoConnect = settingValue(SettingsKey::sipAutoConnect);

  if(autoConnect == 1)
  {
    bindToServer();
  }

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
    if(transport != nullptr)
    {
      if (transport->connection != nullptr)
      {

        transport->connection->exit(0); // stops qthread
        transport->connection->stopConnection(); // exits run loop
        while(transport->connection->isRunning())
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        transport->connection.reset();
      }
      transport->pipe.uninit();
      transport.reset();
    }
  }

  transports_.clear();

  for(auto& dialog : dialogs_)
  {
    dialog.second->pipe.uninit();
  }

  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;
}


// reserve sessionID for a future call
uint32_t SIPManager::reserveSessionID()
{
  ++nextSessionID_;
  return nextSessionID_ - 1;
}


void SIPManager::updateCallSettings()
{
  int autoConnect = settingValue(SettingsKey::sipAutoConnect);
  if(autoConnect == 1)
  {
    bindToServer();
  }

  nCandidates_->init();
}


void SIPManager::bindToServer()
{
  // get server address from settings and bind to server.
  QString serverAddress = settingString(SettingsKey::sipServerAddress);

  if (serverAddress != "" && haveWeRegistered() == "")
  {
    std::shared_ptr<TCPConnection> connection =
        std::shared_ptr<TCPConnection>(new TCPConnection());
    createSIPTransport(serverAddress, connection, true);

    waitingToBind_.push_back(serverAddress);
  }
  else {
    Logger::getLogger()->printWarning(this, "SIP Registrar was empty "
                       "or we have already registered. No registering.");
  }
}


uint32_t SIPManager::startCall(NameAddr &remote)
{
  uint32_t sessionID = reserveSessionID();

  QString connectionAddress = remote.uri.hostport.host;
  QString ourProxyAddress = haveWeRegistered();

  // check if this should be a peer-to-peer connection or if we should use proxy
  bool useOurProxy = shouldUseProxy(remote.uri.hostport.host);

  if (useOurProxy)
  {
    Logger::getLogger()->printNormal(this, "Using our proxy address", "Address", ourProxyAddress);
    connectionAddress = ourProxyAddress;
  }

  // check if we already are connected where we want to send the message
  if (!isConnected(connectionAddress))
  {
    Logger::getLogger()->printNormal(this, "Not connected when starting call. Have to init connection first");
    if (waitingToStart_.find(connectionAddress) != waitingToStart_.end())
    {
      Logger::getLogger()->printProgramError(this, "We already have a waiting to start call "
                              "for this connection, even though it is new");
    }

    // start creation of connection
    std::shared_ptr<TCPConnection> connection =
        std::shared_ptr<TCPConnection>(new TCPConnection());

    // we are not yet connected to them. Form a connection by creating the transport layer
    createSIPTransport(connectionAddress, connection, true);

    // this start call will commence once the connection has been established
    waitingToStart_[connectionAddress] = {sessionID, remote};
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Using existing connection.");

    QString localAddress = getTransport(connectionAddress)->connection->localAddress();

    // get correct local URI
    NameAddr local = localInfo(useOurProxy, localAddress);

    createDialog(sessionID, local, remote, localAddress, true);

    // sends INVITE
    dialogs_[sessionID]->client->sendINVITE(INVITE_TIMEOUT);
  }

  return sessionID;
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


void SIPManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
    dialog->client->sendCANCEL();
    //removeDialog(sessionID);
  }
  else
  {
    Logger::getLogger()->printProgramWarning(this, "Tried to remove a non-existing dialog");
  }
}


void SIPManager::endCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
  dialog->client->sendBYE();

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

  int attempts = 100;
  while (!con->isConnected() && attempts > 0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    -- attempts;
  }

  if (!con->isConnected())
  {
    Logger::getLogger()->printWarning(this, "Did not manage to connect incoming connection!");
    return;
  }

  createSIPTransport(con->remoteAddress(), con, false);
}


void SIPManager::connectionEstablished(QString localAddress, QString remoteAddress)
{
  Q_UNUSED(localAddress)

  // if we are planning to call a peer using this connection
  if (waitingToStart_.find(remoteAddress) != waitingToStart_.end())
  {
    WaitingStart startingSession = waitingToStart_[remoteAddress];
    waitingToStart_.erase(remoteAddress);

    QString localAddress = getTransport(remoteAddress)->connection->localAddress();

    NameAddr local = localInfo(shouldUseProxy(remoteAddress), localAddress);

    uint32_t sessionID = startingSession.sessionID;
    createDialog(sessionID, local, startingSession.contact, localAddress, true);
    dialogs_[sessionID]->client->sendINVITE(INVITE_TIMEOUT);
  }

  // if we are planning to register using this connection
  if(waitingToBind_.contains(remoteAddress))
  {
    NameAddr local = localInfo();

    createRegistration(local);

    getRegistration(remoteAddress)->client->sendREGISTER(REGISTER_INTERVAL);

    waitingToBind_.removeOne(remoteAddress);
  }
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
        localAddress = transports_.first()->connection->localAddress();
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
    foundTransport = transports_[address];
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Could not find transport",
                      "Address", address);
  }

  return foundTransport;
}


void SIPManager::createSIPTransport(QString remoteAddress,
                                    std::shared_ptr<TCPConnection> connection,
                                    bool startConnection)
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

    if (DEFAULT_TRANSPORT == TCP)
    {
      // this informs us when the connection has been established
      QObject::connect(instance->connection.get(), &TCPConnection::socketConnected,
                       this,                       &SIPManager::connectionEstablished);

      // get those network messages to transport
      QObject::connect(instance->connection.get(), &TCPConnection::messageAvailable,
                       transport.get(),            &SIPTransport::networkPackage);

      // get those network messages to transport
      QObject::connect(transport.get(),             &SIPTransport::sendMessage,
                       instance->connection.get(),  &TCPConnection::sendPacket);

      if (startConnection)
      {
        instance->connection->establishConnection(remoteAddress, sipPort_);
      }
    }
    else
    {
      Logger::getLogger()->printUnimplemented(this, "Non-TCP connection in transport creation");
    }

    std::shared_ptr<SIPRouting> routing =
        std::shared_ptr<SIPRouting> (new SIPRouting(instance->connection));

    std::shared_ptr<SIPAuthentication> authentication =
        std::shared_ptr<SIPAuthentication> (new SIPAuthentication());

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

  // because we didn't know our address for this connection earlier, we set it now
  // The origin may be overwritten by peer if they start the call
  setSDPAddress(localAddress,
                sdp->connection_address,
                sdp->connection_nettype,
                sdp->connection_addrtype);

  generateOrigin(sdp, localAddress, getLocalUsername());

  std::shared_ptr<SDPNegotiation> negotiation =
      std::shared_ptr<SDPNegotiation> (new SDPNegotiation(sdp));
  std::shared_ptr<ICE> ice = std::shared_ptr<ICE> (new ICE(nCandidates_, sessionID));

  QObject::connect(ice.get(),         &ICE::nominationSucceeded,
                   negotiation.get(), &SDPNegotiation::nominationSucceeded,
                   Qt::DirectConnection);

  QObject::connect(ice.get(),         &ICE::nominationFailed,
                   negotiation.get(), &SDPNegotiation::iceNominationFailed);


  dialog->client = std::shared_ptr<SIPClient> (new SIPClient);
  dialog->server = std::shared_ptr<SIPServer> (new SIPServer);
  dialog->callbacks = std::shared_ptr<SIPCallbacks> (new SIPCallbacks(sessionID,
                                                                      requestCallbacks_,
                                                                      responseCallbacks_));

  // Initiatiate all the components of the flow.
  dialog->state = std::shared_ptr<SIPDialogState> (new SIPDialogState);
  dialog->state->init(local, remote, ourDialog);

  QObject::connect(negotiation.get(), &SDPNegotiation::iceNominationSucceeded,
                    this, &SIPManager::nominationSucceeded);

  QObject::connect(negotiation.get(), &SDPNegotiation::iceNominationFailed,
                    this, &SIPManager::nominationFailed);

  // Add all components to the pipe.
  dialog->pipe.addProcessor(dialog->state);
  dialog->pipe.addProcessor(ice);
  dialog->pipe.addProcessor(negotiation);
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
  return !remote.isLoopback() && haveWeRegistered() != "";
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
