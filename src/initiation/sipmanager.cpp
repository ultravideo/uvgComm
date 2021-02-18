#include "sipmanager.h"

#include "common.h"
#include "global.h"

#include <QObject>

const uint32_t FIRSTSESSIONID = 1;

// default for SIP, use 5061 for tls encrypted
const uint16_t SIP_PORT = 5060;

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(SIP_PORT),
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
          qSleep(5);
        }
        transport->connection.reset();
      }
      transport->pipe.uninit();
      transport.reset();
    }
  }

  transports_.clear();
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
    std::shared_ptr<TCPConnection> connection =
        std::shared_ptr<TCPConnection>(new TCPConnection());
    createSIPTransport(serverAddress, connection, true);

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



  // check if we are already connected to remoteaddress
  if (!isConnected(remote.uri.hostport.host))
  {
    std::shared_ptr<TCPConnection> connection =
        std::shared_ptr<TCPConnection>(new TCPConnection());

    // we are not yet connected to them. Form a connection by creating the transport layer
    createSIPTransport(remote.uri.hostport.host, connection, true);
    waitingToStart_[remote.uri.hostport.host] = {sessionID, remote};
  }
  else
  {
    QString localAddress = getTransport(remote.uri.hostport.host)->connection->localAddress();

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

  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
  dialog->call.acceptIncomingCall();
}


void SIPManager::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);

  dialog->call.declineIncomingCall();

  removeDialog(sessionID);
}


void SIPManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
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
  std::shared_ptr<DialogInstance> dialog = getDialog(sessionID);
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


void SIPManager::receiveTCPConnection(std::shared_ptr<TCPConnection> con)
{
  printNormal(this, "Received a TCP connection. Initializing dialog.");
  Q_ASSERT(con);

  int attempts = 100;
  while (!con->isConnected() && attempts > 0)
  {
    qSleep(5);
    -- attempts;
  }

  if (!con->isConnected())
  {
    printWarning(this, "Did not manage to connect incoming connection!");
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
    QString localAddress = getTransport(remoteAddress)->connection->localAddress();

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
    NameAddr local = localInfo(true, getTransport(remoteAddress)->connection->localAddress());

    createRegistration(local);

    getRegistration(remoteAddress)->
        registration.bindToServer(local,
                                  getTransport(remoteAddress)->connection->localAddress(),
                                  getTransport(remoteAddress)->connection->localPort());

    waitingToBind_.removeOne(remoteAddress);
  }
}


void SIPManager::transportRequest(SIPRequest &request, QVariant& content)
{
  printNormal(this, "Initiate sending of a dialog request");

  std::shared_ptr<TransportInstance> transport =
      getTransport(request.requestURI.hostport.host);

  if (transport != nullptr)
  {
    transport->pipe.processOutgoingRequest(request, content);
  }
  else
  {
    printDebug(DEBUG_ERROR,  metaObject()->className(),
               "Tried to send request when we are not connected to request-URI");
  }
}


void SIPManager::transportResponse(SIPResponse &response, QVariant& content)
{
  std::shared_ptr<TransportInstance> transport =
      getTransport(response.message->vias.back().sentBy);

  if (transport != nullptr)
  {
    // send the request with or without SDP
    transport->pipe.processOutgoingResponse(response, content);
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(),
               "Tried to send response with invalid.");
  }
}


void SIPManager::processSIPRequest(SIPRequest& request, QVariant& content)
{
  printNormal(this, "Processing incoming re quest");

  uint32_t sessionID = 0;

  // sets the sessionID if session exists or creates a new session if INVITE
  // returns true if either was successful.
  if (!identifySession(request, sessionID))
  {
    printNormal(this, "No existing dialog found.");

    if(request.method == SIP_INVITE)
    {
      QString localAddress = "";

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
        // if the last via matches the local address
        localAddress = transports_[request.message->vias.last().sentBy]->connection->localAddress();
      }
      else if (haveWeRegistered())
      {
        // use a regisration if we have one as a last resort
        for (auto& registration : registrations_)
        {
          if (registration.second->registration.haveWeRegistered())
          {
            localAddress = transports_[registration.second->registration.getServerAddress()]->
                connection->localAddress();
          }
        }
      }
      else
      {
        printWarning(this, "Couldn't determine our local URI!");
      }

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
  std::shared_ptr<DialogInstance> foundDialog = getDialog(sessionID);

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
  printNormal(this, "Processing incoming respose");

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
  std::shared_ptr<DialogInstance> foundDialog = getDialog(sessionID);

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


std::shared_ptr<DialogInstance> SIPManager::getDialog(uint32_t sessionID) const
{
  std::shared_ptr<DialogInstance> foundDialog = nullptr;
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


std::shared_ptr<RegistrationInstance> SIPManager::getRegistration(QString& address) const
{
  std::shared_ptr<RegistrationInstance> foundRegistration = nullptr;
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


std::shared_ptr<TransportInstance> SIPManager::getTransport(QString& address) const
{
  std::shared_ptr<TransportInstance> foundTransport = nullptr;
  if (transports_.find(address) != transports_.end())
  {
    foundTransport = transports_[address];
  }
  else
  {
    printProgramError(this, "Could not find transport",
                      "Address", address);
  }

  return foundTransport;
}


void SIPManager::createSIPTransport(QString remoteAddress,
                                    std::shared_ptr<TCPConnection> connection,
                                    bool startConnection)
{
  Q_ASSERT(stats_);

  if (transports_.find(remoteAddress) == transports_.end())
  {
    printNormal(this, "Creating SIP transport.", "Previous transports",
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
      printUnimplemented(this, "Non-TCP connection in transport creation");
    }

    std::shared_ptr<SIPRouting> routing =
        std::shared_ptr<SIPRouting> (new SIPRouting(instance->connection));

    // remember that the processors are always added from outgoing to incoming
    instance->pipe.addProcessor(transport);
    instance->pipe.addProcessor(routing);

    QObject::connect(&instance->pipe, &SIPMessageFlow::incomingRequest,
                     this,            &SIPManager::processSIPRequest);

    QObject::connect(&instance->pipe, &SIPMessageFlow::incomingResponse,
                     this,            &SIPManager::processSIPResponse);
  }
  else
  {
    printNormal(this, "Not creating SIP transport since it already exists");
  }
}


void SIPManager::createRegistration(NameAddr& addressRecord)
{
  if (registrations_.find(addressRecord.uri.hostport.host) == registrations_.end())
  {
    std::shared_ptr<RegistrationInstance> registration =
        std::shared_ptr<RegistrationInstance> (new RegistrationInstance);
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
}


void SIPManager::createDialog(uint32_t sessionID, NameAddr &local,
                              NameAddr &remote, QString localAddress, bool ourDialog)
{
  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    printWarning(this, "Previous dialog existed with same sessionID as new one!");
    removeDialog(sessionID);
  }

  // Create all the components of the flow. Currently the pipe components are
  // only stored in pipe.
  std::shared_ptr<DialogInstance> dialog = std::shared_ptr<DialogInstance> (new DialogInstance);
  dialogs_[sessionID] = dialog;

  std::shared_ptr<Negotiation> negotiation =
      std::shared_ptr<Negotiation> (new Negotiation(localAddress));
  std::shared_ptr<ICE> ice = std::shared_ptr<ICE> (new ICE(nCandidates_, sessionID));

  QObject::connect(ice.get(),         &ICE::nominationSucceeded,
                   negotiation.get(), &Negotiation::nominationSucceeded,
                   Qt::DirectConnection);

  QObject::connect(ice.get(),         &ICE::nominationFailed,
                   negotiation.get(), &Negotiation::iceNominationFailed);


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
  dialog->pipe.addProcessor(ice);
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
