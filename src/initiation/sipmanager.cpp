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
    registration.second->uninit();
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


uint32_t SIPManager::startCall(NameAddr &address)
{
  uint32_t sessionID = reserveSessionID();

  // TODO: ask network interface if we can start session

  // check if we are already connected to remoteaddress
  if (!isConnected(address.uri.hostport.host))
  {
    // we are not yet connected to them. Form a connection by creating the transport layer
    std::shared_ptr<SIPTransport> transport = createSIPTransport(address.uri.hostport.host);
    transport->createConnection(DEFAULT_TRANSPORT, address.uri.hostport.host);
    waitingToStart_[address.uri.hostport.host] = {sessionID, address};
  }
  else
  {
    // we have an existing connection already. Send SIP message and start call.
    sendINVITE(address, transports_[address.uri.hostport.host]->getLocalAddress(),
               sessionID, haveWeRegistered());
  }

  return sessionID;
}

void SIPManager::sendINVITE(NameAddr &address, QString localAddress,
                            uint32_t sessionID, bool registered)
{
  printNormal(this, "Intializing a new dialog by sending an INVITE");
  createDialog(sessionID);

  dialogs_[sessionID]->state.createNewDialog(address);

  // in peer-to-peer calls we use the actual network address as local URI
  if (!registered)
  {
    dialogs_[sessionID]->state.setLocalHost(localAddress);
  }

  // this start call will commence once the connection has been established
  dialogs_[sessionID]->call.startCall(address.realname);
}



void SIPManager::acceptCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  printNormal(this, "Accepting call", {"SessionID"}, {QString::number(sessionID)});

  std::shared_ptr<SIPDialog> dialog = getDialog(sessionID);
  dialog->call.acceptIncomingCall();
}


void SIPManager::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  std::shared_ptr<SIPDialog> dialog = getDialog(sessionID);

  dialog->call.declineIncomingCall();

  removeDialog(sessionID);
}


void SIPManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<SIPDialog> dialog = getDialog(sessionID);
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
  std::shared_ptr<SIPDialog> dialog = getDialog(sessionID);
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
      dialog.second->negotiation->uninit();
    }
  }

  nextSessionID_ = FIRSTSESSIONID;
}


void SIPManager::getSDPs(uint32_t sessionID,
                         std::shared_ptr<SDPMessageInfo>& localSDP,
                         std::shared_ptr<SDPMessageInfo>& remoteSDP) const
{
  Q_ASSERT(sessionID);

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    localSDP = getDialog(sessionID)->negotiation->getLocalSDP();
    remoteSDP = getDialog(sessionID)->negotiation->getRemoteSDP();
  }
}


void SIPManager::receiveTCPConnection(TCPConnection *con)
{
  printNormal(this, "Received a TCP connection. Initializing dialog.");
  Q_ASSERT(con);

  qSleep(500); // TODO: Remove this with better achitecture!!!

  std::shared_ptr<SIPTransport> transport = createSIPTransport(con->remoteAddress().toString());
  transport->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
}


void SIPManager::connectionEstablished(QString localAddress, QString remoteAddress)
{
  Q_UNUSED(localAddress)

  if (waitingToStart_.find(remoteAddress) != waitingToStart_.end())
  {
    sendINVITE(waitingToStart_[remoteAddress].contact,
               transports_[remoteAddress]->getLocalAddress(),
               waitingToStart_[remoteAddress].sessionID,
               haveWeRegistered());
  }

  if(waitingToBind_.contains(remoteAddress))
  {
    std::shared_ptr<SIPRegistration> registration =
        std::shared_ptr<SIPRegistration> (new SIPRegistration);

    registration->init(statusView_);
    QObject::connect(registration.get(), &SIPRegistration::transportProxyRequest,
                     this, &SIPManager::transportToProxy);

    registrations_[remoteAddress] = registration;

    registration->bindToServer(remoteAddress,
                               transports_[remoteAddress]->getLocalAddress(),
                               transports_[remoteAddress]->getLocalPort());
  }
}


void SIPManager::transportRequest(uint32_t sessionID, SIPRequest &request)
{
  printNormal(this, "Initiate sending of a dialog request");

  std::shared_ptr<SIPDialog> foundDialog = getDialog(sessionID);

  printNormal(this, "Finished sending of a dialog request");

  QVariant content;
  foundDialog->state.processOutgoingRequest(request, content);
  foundDialog->negotiation->processOutgoingRequest(request, content);

  if (transports_.find(request.requestURI.hostport.host) != transports_.end())
  {
    transports_[request.requestURI.hostport.host]->processOutgoingRequest(request, content);
  }
  else
  {
    printDebug(DEBUG_ERROR,  metaObject()->className(),
               "Tried to send request when we are not connected to request-URI");
  }
}


void SIPManager::transportResponse(uint32_t sessionID, SIPResponse &response)
{
  QString remoteAddress = response.message->vias.back().sentBy;
  if (transports_.find(remoteAddress) != transports_.end())
  {
    // determine if we to attach SDP to our response
    QVariant content;

    getDialog(sessionID)->negotiation->processOutgoingResponse(response, content,
                                                               transports_[remoteAddress]->getLocalAddress());
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
  // TODO: ask network interface if we can start session

  uint32_t sessionID = 0;

  // sets the sessionID if session exists or creates a new session if INVITE
  // returns true if either was successful.
  if (identifySession(request, localAddress, sessionID))
  {
    Q_ASSERT(sessionID);
    if (sessionID != 0)
    {
      Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
      printNormal(this, "Starting to process received SIP Request.");

      // find the dialog which corresponds to the callID and tags received in request
      std::shared_ptr<SIPDialog> foundDialog = getDialog(sessionID);

      foundDialog->negotiation->processIncomingRequest(request, content,
                                                       localAddress);

      QVariant content; // unused
      foundDialog->server->processIncomingRequest(request, content);
      if(!foundDialog->call.shouldBeKeptAlive())
      {
        printNormal(this, "Ending session as a results of request.");
        removeDialog(sessionID);
      }

      printNormal(this, "Finished processing request.",
        {"SessionID"}, {QString::number(sessionID)});
    }
    else
    {
      printDebug(DEBUG_PROGRAM_ERROR, metaObject()->className(),
                  "transactions did not set new sessionID.");
    }
  }
  else
  {
    printDebug(DEBUG_PEER_ERROR, metaObject()->className(),
                "transactions could not identify session.");
  }
}


void SIPManager::processSIPResponse(SIPResponse &response, QVariant& content,
                                    QString localAddress)
{
  for (auto& registration : registrations_)
  {
    if(registration.second->identifyRegistration(response))
    {
      printNormal(this, "Got a response to server message!");
      registration.second->processNonDialogResponse(response);
      return;
    }
  }

  uint32_t sessionID = 0;

  if(!identifySession(response, sessionID))
  {
    printDebug(DEBUG_PEER_ERROR, this, 
               "Could not identify response session");
    return;
  }


  Q_ASSERT(sessionID);
  if (sessionID == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
               "SessionID was 0 in processSIPResponse. Should be detected earlier.");
    return;
  }
  if (response.message == nullptr)
  {
    printProgramError(this, "SIPDialog got a message without header");
    return;
  }

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog = getDialog(sessionID);

  foundDialog->negotiation->processIncomingResponse(response, content,
                                                    localAddress);

  foundDialog->state.processIncomingResponse(response, content);
  foundDialog->client->processIncomingResponse(response, content);

  if (!foundDialog->call.shouldBeKeptAlive())
  {
    removeDialog(sessionID);
  }

  printNormal(this, "Response processing finished",
      {"SessionID"}, {QString::number(sessionID)});
}


std::shared_ptr<SIPTransport> SIPManager::createSIPTransport(QString address)
{
  Q_ASSERT(stats_);

  if (transports_.find(address) != transports_.end())
  {
    printNormal(this, "Not creating transport since it already exists");
    return transports_[address];
  }


  std::shared_ptr<SIPTransport> connection =
      std::shared_ptr<SIPTransport>(new SIPTransport(stats_));

  QObject::connect(connection.get(), &SIPTransport::incomingRequest,
                   this, &SIPManager::processSIPRequest);

  QObject::connect(connection.get(), &SIPTransport::incomingResponse,
                   this, &SIPManager::processSIPResponse);

  QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                   this, &SIPManager::connectionEstablished);
  transports_[address] = connection;

  return transports_[address];
}


bool SIPManager::isConnected(QString remoteAddress)
{
  return transports_.find(remoteAddress) != transports_.end();
}


bool SIPManager::identifySession(SIPRequest& request,
                                 QString localAddress,
                                 uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP Request dialog.");

  out_sessionID = 0;

  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr)
    {
      if ((request.method == SIP_CANCEL && i->second->server->isCANCELYours(request)) ||
          i->second->state.correctRequestDialog(request.message, request.method,
                                                request.message->cSeq.cSeq))
      {
        printNormal(this, "Found dialog matching for incoming request.");
        out_sessionID = i->first;
      }
    }
  }

  // we did not find existing dialog for this request
  if(out_sessionID == 0)
  {
    printNormal(this, "No existing dialog found.");

    // TODO: there is a problem if the sequence number did not match
    // and the request type is INVITE
    if(request.method == SIP_INVITE)
    {
      printNormal(this, "Someone is trying to start a SIP dialog with us!");

      out_sessionID = createDialogFromINVITE(localAddress, request);
      return true;
    }
    return false;
  }
  return true;
}


bool SIPManager::identifySession(SIPResponse &response, uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP response dialog.");

  out_sessionID = 0;
  // find the dialog which corresponds to the callID and tags received in response
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr &&
       i->second->state.correctResponseDialog(response.message, response.message->cSeq.cSeq) &&
       i->second->client->waitingResponse(response.message->cSeq.method))
    {
      printNormal(this, "Found dialog matching the response");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


std::shared_ptr<SIPDialog> SIPManager::getDialog(uint32_t sessionID) const
{
  std::shared_ptr<SIPDialog> foundDialog = nullptr;
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


void SIPManager::createDialog(uint32_t sessionID)
{
  std::shared_ptr<SIPDialog> dialog = std::shared_ptr<SIPDialog> (new SIPDialog);
  dialogs_[sessionID] = dialog;

  dialogs_[sessionID]->client = std::shared_ptr<SIPClient> (new SIPClient);
  dialogs_[sessionID]->server = std::shared_ptr<SIPServer> (new SIPServer);

  dialog->call.init(dialog->client, dialog->server, transactionUser_, sessionID);

  dialog->negotiation = std::shared_ptr<Negotiation> (new Negotiation(nCandidates_,
                                                                      sessionID));

  QObject::connect(&dialogs_[sessionID]->call, &SIPSingleCall::sendDialogRequest,
                   this, &SIPManager::transportRequest);

  QObject::connect(&dialogs_[sessionID]->call, &SIPSingleCall::sendResponse,
                   this, &SIPManager::transportResponse);

  QObject::connect(dialog->negotiation.get(), &Negotiation::iceNominationSucceeded,
                    this, &SIPManager::nominationSucceeded);

  QObject::connect(dialog->negotiation.get(), &Negotiation::iceNominationFailed,
                    this, &SIPManager::nominationFailed);
}


void SIPManager::removeDialog(uint32_t sessionID)
{
  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    getDialog(sessionID)->negotiation->uninit();
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


uint32_t SIPManager::createDialogFromINVITE(QString localAddress,
                                            SIPRequest &invite)
{
  uint32_t sessionID = reserveSessionID();
  createDialog(sessionID);

  QVariant content;
  dialogs_[sessionID]->state.processIncomingRequest(invite, content);
  dialogs_[sessionID]->state.setLocalHost(localAddress);

  return sessionID;
}


bool SIPManager::haveWeRegistered()
{
  for (auto& registration : registrations_)
  {
    if (registration.second->haveWeRegistered())
    {
      return true;
    }
  }
  return false;
}
