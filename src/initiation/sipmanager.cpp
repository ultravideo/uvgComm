#include "sipmanager.h"

#include "common.h"
#include "global.h"

#include <QObject>

const uint32_t FIRSTSESSIONID = 1;


SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transports_(),
  nextSessionID_(FIRSTSESSIONID),
  dialogs_(),
  transactionUser_(nullptr)
{}


// start listening to incoming
void SIPManager::init(SIPTransactionUser* callControl, StatisticsInterface *stats,
                      ServerStatusView *statusView)
{
  QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                   this, &SIPManager::receiveTCPConnection);

  stats_ = stats;
  statusView_ = statusView;

  tcpServer_.setProxy(QNetworkProxy::NoProxy);

  // listen to everything
  printNormal(this, "Listening to SIP TCP connections", "Port",
              QString::number(sipPort_));

  if (!tcpServer_.listen(QHostAddress::Any, sipPort_))
  {
    printDebug(DEBUG_ERROR, this,
               "Failed to listen to socket. Is it reserved?");

    // TODO announce it to user!
  }

  transactionUser_ = callControl;

  QSettings settings("kvazzup.ini", QSettings::IniFormat);


  int autoConnect = settings.value("sip/AutoConnect").toInt();

  if(autoConnect == 1)
  {
    bindToServer();
  }

  nCandidates_ = std::shared_ptr<NetworkCandidates> (new NetworkCandidates);
  nCandidates_->setPortRange(MIN_ICE_PORT, MAX_ICE_PORT);
}


void SIPManager::uninit()
{
  printNormal(this, "Uninit SIP Manager");

  dialogs_.clear();
  nextSessionID_ = FIRSTSESSIONID;

  for (auto& registration : registrations_)
  {
    registration.second->pipe.uninit();
    registration.second->registration.uninit();
  }

  for(std::shared_ptr<SIPTransport> transport : transports_)
  {
    if(transport != nullptr)
    {
      transport->cleanup();
      transport.reset();
    }
  }
}


// reserve sessionID for a future call
uint32_t SIPManager::reserveSessionID()
{
  ++nextSessionID_;
  return nextSessionID_ - 1;
}


void SIPManager::updateSettings()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  int autoConnect = settings.value("sip/AutoConnect").toInt();
  if(autoConnect == 1)
  {
    bindToServer();
  }
}


void SIPManager::bindToServer()
{
  // get server address from settings and bind to server.
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  QString serverAddress = settings.value("sip/ServerAddress").toString();

  if (serverAddress != "" && !haveWeRegistered())
  {
    std::shared_ptr<SIPTransport> transport = createSIPTransport(serverAddress);
    transport->createConnection(DEFAULT_TRANSPORT, serverAddress);

    waitingToBind_.push_back(serverAddress);
  }
  else {
    printWarning(this, "SIP Registrar was empty "
                       "or we have already registered. No registering.");
  }
}


uint32_t SIPManager::startCall(NameAddr &remote)
{
  uint32_t sessionID = reserveSessionID();

  // TODO: ask network interface if we can start session

  // check if we are already connected to remoteaddress
  if (!isConnected(remote.uri.hostport.host))
  {
    // we are not yet connected to them. Form a connection by creating the transport layer
    std::shared_ptr<SIPTransport> transport = createSIPTransport(remote.uri.hostport.host);
    transport->createConnection(DEFAULT_TRANSPORT, remote.uri.hostport.host);
    waitingToStart_[remote.uri.hostport.host] = {sessionID, remote};
  }
  else
  {
    QString localAddress = transports_[remote.uri.hostport.host]->getLocalAddress();

    NameAddr local = localInfo(haveWeRegistered(), localAddress);

    createDialog(sessionID, local, remote, localAddress, true);

    // this start call will commence once the connection has been established
    dialogs_[sessionID]->call.startCall(remote.realname);
  }

  return sessionID;
}


void SIPManager::acceptCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  printNormal(this, "Accepting call", {"SessionID"}, {QString::number(sessionID)});

  std::shared_ptr<DialogData> dialog = getDialog(sessionID);
  dialog->call.acceptIncomingCall();
}


void SIPManager::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<DialogData> dialog = getDialog(sessionID);

  dialog->call.declineIncomingCall();

  removeDialog(sessionID);
}


void SIPManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<DialogData> dialog = getDialog(sessionID);
    dialog->call.cancelOutgoingCall();
    removeDialog(sessionID);
  }
  else
  {
    printProgramWarning(this, "Tried to remove a non-existing dialog");
  }
}


void SIPManager::endCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<DialogData> dialog = getDialog(sessionID);
  dialog->call.endCall();

  removeDialog(sessionID);
}


void SIPManager::endAllCalls()
{
  for(auto& dialog : dialogs_)
  {
    if(dialog.second != nullptr)
    {
      dialog.second->call.endCall();

      // TODO: This should be done later when we receive OK
      //dialog.second->pipe.uninit();
    }
  }

  nextSessionID_ = FIRSTSESSIONID;
}


void SIPManager::receiveTCPConnection(TCPConnection *con)
{
  printNormal(this, "Received a TCP connection. Initializing dialog.");
  Q_ASSERT(con);

  qSleep(500); // TODO: Remove this with better achitecture!!!

  std::shared_ptr<SIPTransport> transport =
      createSIPTransport(con->remoteAddress().toString());

  transport->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
}


void SIPManager::connectionEstablished(QString localAddress, QString remoteAddress)
{
  Q_UNUSED(localAddress)

  // if we are planning to call a peer using this connection
  if (waitingToStart_.find(remoteAddress) != waitingToStart_.end())
  {
    QString localAddress = transports_[remoteAddress]->getLocalAddress();

    NameAddr local = localInfo(haveWeRegistered(), localAddress);

    uint32_t sessionID = waitingToStart_[remoteAddress].sessionID;
    createDialog(sessionID, local, waitingToStart_[remoteAddress].contact, localAddress, true);
    dialogs_[sessionID]->call.startCall(waitingToStart_[remoteAddress].contact.realname);

    waitingToStart_.erase(remoteAddress);
  }

  // TODO: Fix this according to new architecture
  // if we are planning to register using this connection
  if(waitingToBind_.contains(remoteAddress))
  {
    NameAddr local = localInfo(true, transports_[remoteAddress]->getLocalAddress());

    createRegistration(local);

    getRegistration(remoteAddress)->
        registration.bindToServer(local,
                                  transports_[remoteAddress]->getLocalAddress(),
                                  transports_[remoteAddress]->getLocalPort());

    waitingToBind_.removeOne(remoteAddress);
  }
}


void SIPManager::transportRequest(SIPRequest &request, QVariant& content)
{
  printNormal(this, "Initiate sending of a dialog request");

  if (transports_.find(request.requestURI.hostport.host) != transports_.end())
  {
    transports_[request.requestURI.hostport.host]->processOutgoingRequest(request,
                                                                          content);
  }
  else
  {
    printDebug(DEBUG_ERROR,  metaObject()->className(),
               "Tried to send request when we are not connected to request-URI");
  }
}


void SIPManager::transportResponse(SIPResponse &response, QVariant& content)
{
  QString remoteAddress = response.message->vias.back().sentBy;
  if (transports_.find(remoteAddress) != transports_.end())
  {
    // send the request with or without SDP
    transports_[remoteAddress]->processOutgoingResponse(response, content);
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(),
               "Tried to send response with invalid.");
  }
}


void SIPManager::transportToProxy(QString serverAddress, SIPRequest &request)
{
  if (transports_.find(serverAddress) != transports_.end())
  {
    // send the request without content
    QVariant content;
    transports_[serverAddress]->processOutgoingRequest(request, content);
  }
}


void SIPManager::processSIPRequest(SIPRequest& request, QVariant& content,
                                   QString localAddress)
{
  uint32_t sessionID = 0;

  // sets the sessionID if session exists or creates a new session if INVITE
  // returns true if either was successful.
  if (!identifySession(request, sessionID))
  {
    printNormal(this, "No existing dialog found.");

    // TODO: there is a problem if the sequence number did not match
    // and the request type is INVITE
    if(request.method == SIP_INVITE)
    {
      printNormal(this, "Someone is trying to start a SIP dialog with us!");
      sessionID = reserveSessionID();

      // if they used our local address in to, we use that also, otherwise we use our binding
      NameAddr local = localInfo(localAddress != request.message->to.address.uri.hostport.host,
                                 localAddress);

      createDialog(sessionID, local, request.message->from.address, localAddress, false);
    }
    else
    {
      printUnimplemented(this, "Out of Dialog request");
      return;
    }
  }

  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  printNormal(this, "Starting to process received SIP Request.");

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<DialogData> foundDialog = getDialog(sessionID);

  foundDialog->pipe.processIncomingRequest(request, content);

  if(!foundDialog->call.shouldBeKeptAlive())
  {
    printNormal(this, "Ending session as a results of request.");
    removeDialog(sessionID);
  }

  printNormal(this, "Finished processing request.",
    {"SessionID"}, {QString::number(sessionID)});
}


void SIPManager::processSIPResponse(SIPResponse &response, QVariant& content)
{
  if (response.message == nullptr)
  {
    printProgramError(this, "Process response got a message without header");
    return;
  }

  for (auto& registration : registrations_)
  {
    if(registration.second->state->correctResponseDialog(response.message->callID,
                                                         response.message->to.tagParameter,
                                                         response.message->from.tagParameter))
    {
      printNormal(this, "Got a response to server message!");
      registration.second->pipe.processIncomingResponse(response, content);
      return;
    }
  }

  uint32_t sessionID = 0;

  if(!identifySession(response, sessionID) || sessionID == 0)
  {
    printDebug(DEBUG_PEER_ERROR, this, 
               "Could not identify response session");
    return;
  }

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<DialogData> foundDialog = getDialog(sessionID);

  foundDialog->pipe.processIncomingResponse(response, content);

  if (!foundDialog->call.shouldBeKeptAlive())
  {
    removeDialog(sessionID);
  }

  printNormal(this, "Response processing finished",
      {"SessionID"}, {QString::number(sessionID)});
}


bool SIPManager::isConnected(QString remoteAddress)
{
  return transports_.find(remoteAddress) != transports_.end();
}


bool SIPManager::identifySession(SIPRequest& request,
                                 uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP Request dialog.");

  out_sessionID = 0;

  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if (i->second != nullptr &&
        i->second->state->correctRequestDialog(request.message->callID,
                                               request.message->to.tagParameter,
                                               request.message->from.tagParameter))
    {
      printNormal(this, "Found matching dialog for incoming request.");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


bool SIPManager::identifySession(SIPResponse &response, uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP response dialog.");

  out_sessionID = 0;
  // find the dialog which corresponds to the callID and tags received in response
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if (i->second != nullptr &&
        i->second->state->correctResponseDialog(response.message->callID,
                                                response.message->to.tagParameter,
                                                response.message->from.tagParameter))
    {
      printNormal(this, "Found matching dialog for incoming response");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


std::shared_ptr<DialogData> SIPManager::getDialog(uint32_t sessionID) const
{
  std::shared_ptr<DialogData> foundDialog = nullptr;
  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    foundDialog = dialogs_.at(sessionID);
  }
  else
  {
    printProgramError(this, "Could not find dialog",
                      "SessionID", QString::number(sessionID));
  }

  return foundDialog;
}


std::shared_ptr<RegistrationData> SIPManager::getRegistration(QString& address) const
{
  std::shared_ptr<RegistrationData> foundRegistration = nullptr;
  if (registrations_.find(address) != registrations_.end())
  {
    foundRegistration = registrations_.at(address);
  }
  else
  {
    printProgramError(this, "Could not find registration",
                      "Address", address);
  }

  return foundRegistration;
}


std::shared_ptr<SIPTransport> SIPManager::createSIPTransport(QString address)
{
  Q_ASSERT(stats_);

  if (transports_.find(address) == transports_.end())
  {
    printNormal(this, "Creating SIP transport.");

    std::shared_ptr<SIPTransport> connection =
        std::shared_ptr<SIPTransport>(new SIPTransport(stats_));

    QObject::connect(connection.get(), &SIPTransport::incomingRequest,
                     this, &SIPManager::processSIPRequest);

    QObject::connect(connection.get(), &SIPTransport::incomingResponse,
                     this, &SIPManager::processSIPResponse);

    QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                     this, &SIPManager::connectionEstablished);
    transports_[address] = connection;
  }

  return transports_[address];
}


void SIPManager::createRegistration(NameAddr& addressRecord)
{
  std::shared_ptr<RegistrationData> registration =
      std::shared_ptr<RegistrationData> (new RegistrationData);
  registrations_[addressRecord.uri.hostport.host] = registration;

  registration->registration.init(statusView_);

  registration->state = std::shared_ptr<SIPDialogState> (new SIPDialogState);

  SIP_URI serverUri = {DEFAULT_SIP_TYPE, {"", ""},
                       {addressRecord.uri.hostport.host, 0}, {}, {}};
  registration->state->createServerConnection(addressRecord, serverUri);


  std::shared_ptr<SIPClient> client = std::shared_ptr<SIPClient> (new SIPClient);
  //std::shared_ptr<SIPServer> server = std::shared_ptr<SIPServer> (new SIPServer);

  // Add all components to the pipe.
  registration->pipe.addProcessor(registration->state);
  registration->pipe.addProcessor(client);
  //registration->pipe.addProcessor(server);


  // Connect the pipe to registration and transmission functions.
  registration->registration.connectOutgoingProcessor(registration->pipe);
  registration->pipe.connectIncomingProcessor(registration->registration);

  QObject::connect(&registration->pipe, &SIPMessageFlow::outgoingRequest,
                   this, &SIPManager::transportRequest);

  QObject::connect(&registration->pipe, &SIPMessageFlow::outgoingResponse,
                   this, &SIPManager::transportResponse);
}


void SIPManager::createDialog(uint32_t sessionID, NameAddr &local,
                              NameAddr &remote, QString localAddress, bool ourDialog)
{
  // Create all the components of the flow. Currently the pipe components are
  // only stored in pipe.
  std::shared_ptr<DialogData> dialog = std::shared_ptr<DialogData> (new DialogData);
  dialogs_[sessionID] = dialog;

  std::shared_ptr<Negotiation> negotiation =
      std::shared_ptr<Negotiation> (new Negotiation(nCandidates_, localAddress, sessionID));
  std::shared_ptr<SIPClient> client = std::shared_ptr<SIPClient> (new SIPClient);
  std::shared_ptr<SIPServer> server = std::shared_ptr<SIPServer> (new SIPServer);

  // Initiatiate all the components of the flow.
  dialog->call.init(transactionUser_, sessionID);
  dialog->state = std::shared_ptr<SIPDialogState> (new SIPDialogState);
  dialog->state->init(local, remote, ourDialog);

  QObject::connect(negotiation.get(), &Negotiation::iceNominationSucceeded,
                    this, &SIPManager::nominationSucceeded);

  QObject::connect(negotiation.get(), &Negotiation::iceNominationFailed,
                    this, &SIPManager::nominationFailed);

  // Add all components to the pipe.
  dialog->pipe.addProcessor(dialog->state);
  dialog->pipe.addProcessor(negotiation);
  dialog->pipe.addProcessor(client);
  dialog->pipe.addProcessor(server);

  // Connect the pipe to call and transmission functions.
  dialog->call.connectOutgoingProcessor(dialog->pipe);
  dialog->pipe.connectIncomingProcessor(dialog->call);

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
    // TODO: This may cause problems if we have reserved a sessionID, but have not yet
    // started a new one. Maybe remove this when statistics has been updated with new
    // map for tracking sessions.
    nextSessionID_ = FIRSTSESSIONID;
  }
}


bool SIPManager::haveWeRegistered()
{
  for (auto& registration : registrations_)
  {
    if (registration.second->registration.haveWeRegistered())
    {
      return true;
    }
  }
  return false;
}


NameAddr SIPManager::localInfo(bool registered, QString connectionAddress)
{
  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  NameAddr local;

  local.realname = settings.value("local/Name").toString();
  local.uri.userinfo.user = getLocalUsername();

  // dont set server address if we have already set peer-to-peer address
  if (registered)
  {
    local.uri.hostport.host = settings.value("sip/ServerAddress").toString();
  }
  else
  {
    local.uri.hostport.host = connectionAddress;
  }

  local.uri.type = DEFAULT_SIP_TYPE;
  local.uri.hostport.port = 0; // port is added later if needed

  if(local.uri.userinfo.user.isEmpty())
  {
    local.uri.userinfo.user = "anonymous";
  }

  return local;
}
