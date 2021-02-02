#include "sipmanager.h"

#include "common.h"
#include "global.h"

#include <QObject>


const quint32 FIRSTTRANSPORTID = 1;

const uint32_t FIRSTSESSIONID = 1;

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transports_(),
  nextTransportID_(FIRSTTRANSPORTID),
  negotiations_(),
  pendingConnectionMutex_(),
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
  registrations_.init(statusView);

  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  QObject::connect(&registrations_, &SIPRegistrations::transportProxyRequest,
                   this, &SIPManager::transportToProxy);

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
  registrations_.uninit();

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


void SIPManager::uninitSession(uint32_t sessionID)
{
  negotiations_.at(sessionID)->endSession();

  // if you are wondering why the dialog is not destroy here, the dialog is
  // often the start point of these kinds of destructions and removing dialog
  // here would corrupt the heap. Instead dialog is destroyed by return values
  // in dialog manager
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

  if (serverAddress != "" && !registrations_.haveWeRegistered())
  {
    std::shared_ptr<SIPTransport> transport = createSIPTransport();
    transport->createConnection(DEFAULT_TRANSPORT, serverAddress);

    serverToTransportID_[serverAddress] = transport->getTransportID();

    waitingToBind_[transport->getTransportID()] = serverAddress;
  }
  else {
    printWarning(this, "SIP Registrar was empty "
                       "or we have already registered. No registering.");
  }
}


uint32_t SIPManager::startCall(NameAddr &address)
{
  quint32 transportID = 0;
  uint32_t sessionID = reserveSessionID();

  createNegotiation(sessionID);

  // TODO: ask network interface if we can start session

  // check if we are already connected to remoteaddress and set transportID
  if (!isConnected(address.uri.hostport.host, transportID))
  {
    // we are not yet connected to them. Form a connection by creating the transport layer
    std::shared_ptr<SIPTransport> transport = createSIPTransport();
    transportID = transport->getTransportID(); // Get new transportID
    sessionToTransportID_[sessionID] = transportID;
    transport->createConnection(DEFAULT_TRANSPORT, address.uri.hostport.host);
    waitingToStart_[transportID] = {sessionID, address};
  }
  else {
    // associate this sessionID with existing transportID
    sessionToTransportID_[sessionID] = transportID;
    // we have an existing connection already. Send SIP message and start call.
    sendINVITE(address, transports_[transportID]->getLocalAddress(),
               sessionID, registrations_.haveWeRegistered());
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
  if(!dialogs_[sessionID]->client.transactionINVITE(address.realname, INVITE_TIMEOUT))
  {
    printWarning(this, "Could not start a call according to client.");
  }
}



void SIPManager::acceptCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  printNormal(this, "Accepting call", {"SessionID"}, {QString::number(sessionID)});

  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  dialog->server.respond(SIP_OK);
}


void SIPManager::rejectCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->server.respond(SIP_DECLINE);

  removeDialog(sessionID);
}


void SIPManager::cancelCall(uint32_t sessionID)
{
  Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());

  if (dialogs_.find(sessionID) != dialogs_.end())
  {
    std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
    dialog->client.transactionCANCEL();
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
  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> dialog = dialogs_[sessionID];
  dialogMutex_.unlock();
  dialog->client.transactionBYE();

  removeDialog(sessionID);

  if (negotiations_.find(sessionID) != negotiations_.end())
  {
    negotiations_[sessionID]->endSession();
  }
}


void SIPManager::endAllCalls()
{
  for(auto& dialog : dialogs_)
  {
    if(dialog.second != nullptr)
    {
      dialog.second->client.transactionBYE();
    }
  }

  nextSessionID_ = FIRSTSESSIONID;

  for (auto& negotiation : negotiations_)
  {
    negotiation.second->endSession();
  }

  negotiations_.clear();
}


void SIPManager::getSDPs(uint32_t sessionID,
                         std::shared_ptr<SDPMessageInfo>& localSDP,
                         std::shared_ptr<SDPMessageInfo>& remoteSDP) const
{
  Q_ASSERT(sessionID);

  if (negotiations_.find(sessionID) != negotiations_.end())
  {
    localSDP = negotiations_.at(sessionID)->getLocalSDP();
    remoteSDP = negotiations_.at(sessionID)->getRemoteSDP();
  }
}


void SIPManager::receiveTCPConnection(TCPConnection *con)
{
  printNormal(this, "Received a TCP connection. Initializing dialog.");
  Q_ASSERT(con);

  std::shared_ptr<SIPTransport> transport = createSIPTransport();
  transport->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
}


void SIPManager::connectionEstablished(quint32 transportID)
{
  if (waitingToStart_.find(transportID) != waitingToStart_.end())
  {
    sendINVITE(waitingToStart_[transportID].contact,
               transports_[transportID]->getLocalAddress(),
               waitingToStart_[transportID].sessionID,
               registrations_.haveWeRegistered());
  }

  if(waitingToBind_.find(transportID) != waitingToBind_.end())
  {
    registrations_.bindToServer(waitingToBind_[transportID],
                                transports_[transportID]->getLocalAddress(),
                                transports_[transportID]->getLocalPort());
  }
}


void SIPManager::transportRequest(uint32_t sessionID, SIPRequest &request)
{
  printNormal(this, "Initiate sending of a dialog request");

  dialogMutex_.lock();
  std::shared_ptr<SIPDialog> foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  QVariant content; // unused
  foundDialog->state.processOutgoingRequest(request, content);

  printNormal(this, "Finished sending of a dialog request");

  if (sessionToTransportID_.find(sessionID) != sessionToTransportID_.end())
  {
    quint32 transportID = sessionToTransportID_[sessionID];

    if (transports_.find(transportID) != transports_.end())
    {
      QVariant content;
      if (negotiations_.find(sessionID) != negotiations_.end())
      {
        negotiations_.at(sessionID)->processOutgoingRequest(request, content);
      }
      transports_[transportID]->processOutgoingRequest(request, content);
    }
    else {
      printDebug(DEBUG_ERROR,  metaObject()->className(), 
                 "Tried to send request with invalid transportID");
    }
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(), 
                "No mapping from sessionID to transportID");
  }
}


void SIPManager::transportResponse(uint32_t sessionID, SIPResponse &response)
{
  if (sessionToTransportID_.find(sessionID) != sessionToTransportID_.end())
  {
    quint32 transportID = sessionToTransportID_[sessionID];

    if (transports_.find(transportID) != transports_.end())
    {
      // determine if we to attach SDP to our response
      QVariant content;
      if (negotiations_.find(sessionID) != negotiations_.end())
      {
        negotiations_.at(sessionID)->processOutgoingResponse(response, content,
                                                             transports_[transportID]->getLocalAddress());
      }

      // send the request with or without SDP
      transports_[transportID]->processOutgoingResponse(response, content);
    }
    else {
      printDebug(DEBUG_ERROR, metaObject()->className(), 
                 "Tried to send response with invalid.", {"transportID"},
      {QString::number(transportID)});
    }
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(), 
                "No mapping from sessionID to transportID");
  }
}


void SIPManager::transportToProxy(QString serverAddress, SIPRequest &request)
{
  if (serverToTransportID_.find(serverAddress) != serverToTransportID_.end())
  {
    quint32 transportID = serverToTransportID_[serverAddress];

    if (transports_.find(transportID) != transports_.end())
    {
      // send the request without content
      QVariant content;
      transports_[transportID]->processOutgoingRequest(request, content);
    }
  }
}


void SIPManager::processSIPRequest(SIPRequest& request, QString localAddress,
                                   QVariant& content, quint32 transportID)
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
      sessionToTransportID_[sessionID] = transportID;

      if (negotiations_.find(sessionID) == negotiations_.end())
      {
        createNegotiation(sessionID);
      }

      negotiations_.at(sessionID)->processIncomingRequest(request, content,
                                                          transports_[transportID]->getLocalAddress());

      Q_ASSERT(dialogs_.find(sessionID) != dialogs_.end());
      printNormal(this, "Starting to process received SIP Request.");

      dialogMutex_.lock();

      // find the dialog which corresponds to the callID and tags received in request
      std::shared_ptr<SIPDialog> foundDialog;
      foundDialog = dialogs_[sessionID];
      dialogMutex_.unlock();

      QVariant content; // unused
      foundDialog->server.processIncomingRequest(request, content);
      if(!foundDialog->server.shouldBeKeptAlive())
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


void SIPManager::processSIPResponse(SIPResponse &response, QVariant& content)
{
  QString possibleServerAddress = "";
  if(registrations_.identifyRegistration(response, possibleServerAddress))
  {
    printNormal(this, "Got a response to server message!");
    registrations_.processNonDialogResponse(response);
    return;
  }

  uint32_t sessionID = 0;

  if(!identifySession(response, sessionID))
  {
    printDebug(DEBUG_PEER_ERROR, this, 
               "Could not identify response session");
    return;
  }

  if(sessionToTransportID_.find(sessionID) == sessionToTransportID_.end())
  {
    printDebug(DEBUG_WARNING, this,
               "Could not identify transport for session");
    return;
  }

  if (negotiations_.find(sessionID) == negotiations_.end())
  {
    createNegotiation(sessionID);
  }

  quint32 transportID = sessionToTransportID_[sessionID];
  negotiations_.at(sessionID)->processIncomingResponse(response, content,
                                                       transports_[transportID]->getLocalAddress());

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

  dialogMutex_.lock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;
  foundDialog = dialogs_[sessionID];
  dialogMutex_.unlock();

  foundDialog->state.processIncomingResponse(response, content);
  foundDialog->client.processIncomingResponse(response, content);

  if (!foundDialog->client.shouldBeKeptAlive())
  {
    removeDialog(sessionID);
  }

  printNormal(this, "Response processing finished",
      {"SessionID"}, {QString::number(sessionID)});
}


std::shared_ptr<SIPTransport> SIPManager::createSIPTransport()
{
  Q_ASSERT(stats_);

  quint32 transportID = nextTransportID_;
  ++nextTransportID_;

  std::shared_ptr<SIPTransport> connection =
      std::shared_ptr<SIPTransport>(new SIPTransport(transportID, stats_));

  QObject::connect(connection.get(), &SIPTransport::incomingRequest,
                   this, &SIPManager::processSIPRequest);

  QObject::connect(connection.get(), &SIPTransport::incomingResponse,
                   this, &SIPManager::processSIPResponse);

  QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                   this, &SIPManager::connectionEstablished);
  transports_[transportID] = connection;

  return connection;
}


bool SIPManager::isConnected(QString remoteAddress, quint32& outTransportID)
{
  for(auto& transport : transports_)
  {
    if(transport != nullptr &&
       transport->getRemoteAddress() == remoteAddress)
    {
      outTransportID = transport->getTransportID();
      return true;
    }
  }
  return false;
}


void SIPManager::createNegotiation(uint32_t sessionID)
{
  negotiations_[sessionID] = std::shared_ptr<Negotiation> (new Negotiation(nCandidates_, sessionID));

  QObject::connect(negotiations_[sessionID].get(), &Negotiation::iceNominationSucceeded,
                    this, &SIPManager::nominationSucceeded);
  QObject::connect(negotiations_[sessionID].get(), &Negotiation::iceNominationFailed,
                    this, &SIPManager::nominationFailed);
}


bool SIPManager::identifySession(SIPRequest& request,
                                       QString localAddress,
                                       uint32_t& out_sessionID)
{
  printNormal(this, "Starting to process identifying SIP Request dialog.");

  out_sessionID = 0;

  dialogMutex_.lock();
  for (auto i = dialogs_.begin(); i != dialogs_.end(); ++i)
  {
    if(i->second != nullptr)
    {
      if ((request.method == SIP_CANCEL && i->second->server.isCANCELYours(request)) ||
          i->second->state.correctRequestDialog(request.message, request.method,
                                                request.message->cSeq.cSeq))
      {
        printNormal(this, "Found dialog matching for incoming request.");
        out_sessionID = i->first;
      }
    }
  }
  dialogMutex_.unlock();

  // find the dialog which corresponds to the callID and tags received in request
  std::shared_ptr<SIPDialog> foundDialog;

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
       i->second->client.waitingResponse(response.message->cSeq.method))
    {
      printNormal(this, "Found dialog matching the response");
      out_sessionID = i->first;
      return true;
    }
  }

  return false;
}


void SIPManager::createDialog(uint32_t sessionID)
{
  std::shared_ptr<SIPDialog> dialog = std::shared_ptr<SIPDialog> (new SIPDialog);

  dialogMutex_.lock();
  dialogs_[sessionID] = dialog;
  dialogMutex_.unlock();

  dialogs_[sessionID]->client.setDialogStuff(transactionUser_, sessionID);
  dialogs_[sessionID]->server.init(transactionUser_, sessionID);

  QObject::connect(&dialogs_[sessionID]->client, &SIPClient::sendDialogRequest,
                   this, &SIPManager::transportRequest);

  QObject::connect(&dialogs_[sessionID]->client, &SIPClient::BYETimeout,
                   this, &SIPManager::removeDialog);

  QObject::connect(&dialogs_[sessionID]->server, &SIPServer::sendResponse,
                   this, &SIPManager::transportResponse);
}


void SIPManager::removeDialog(uint32_t sessionID)
{
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
