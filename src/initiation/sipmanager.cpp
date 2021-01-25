#include "sipmanager.h"

#include <QObject>


const quint32 FIRSTTRANSPORTID = 1;

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transports_(),
  nextTransportID_(FIRSTTRANSPORTID),
  dialogManager_(),
  negotiation_()
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
  printNormal(this, "Listening to SIP TCP connections", "Port", QString::number(sipPort_));

  if (!tcpServer_.listen(QHostAddress::Any, sipPort_))
  {
    printDebug(DEBUG_ERROR, this,
               "Failed to listen to socket. Is it reserved?");

    // TODO announce it to user!
  }

  dialogManager_.init(callControl);
  registrations_.init(statusView);

  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  negotiation_.init();

  QObject::connect(&dialogManager_, &SIPDialogManager::transportRequest,
                   this, &SIPManager::transportRequest);
  QObject::connect(&dialogManager_, &SIPDialogManager::transportResponse,
                   this, &SIPManager::transportResponse);
  QObject::connect(&negotiation_, &Negotiation::iceNominationSucceeded,
                    this, &SIPManager::nominationSucceeded);
  QObject::connect(&negotiation_, &Negotiation::iceNominationFailed,
                    this, &SIPManager::nominationFailed);
  QObject::connect(&registrations_, &SIPRegistrations::transportProxyRequest,
                   this, &SIPManager::transportToProxy);

  int autoConnect = settings.value("sip/AutoConnect").toInt();

  if(autoConnect == 1)
  {
    bindToServer();
  }
}


void SIPManager::uninit()
{
  printNormal(this, "Uninit SIP Manager");

  dialogManager_.uninit();
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


void SIPManager::uninitSession(uint32_t sessionID)
{
  negotiation_.endSession(sessionID);

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
  uint32_t sessionID = dialogManager_.reserveSessionID();

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
    dialogManager_.startCall(address,
                            transports_[transportID]->getLocalAddress(),
                            sessionID, registrations_.haveWeRegistered());
  }

  return sessionID;
}


void SIPManager::acceptCall(uint32_t sessionID)
{
  dialogManager_.acceptCall(sessionID);
}


void SIPManager::rejectCall(uint32_t sessionID)
{
  dialogManager_.rejectCall(sessionID);
}


void SIPManager::cancelCall(uint32_t sessionID)
{
  dialogManager_.cancelCall(sessionID);
}


void SIPManager::endCall(uint32_t sessionID)
{
  dialogManager_.endCall(sessionID);
  negotiation_.endSession(sessionID);
}


void SIPManager::endAllCalls()
{
  dialogManager_.endAllCalls();
  negotiation_.endAllSessions();
}


void SIPManager::getSDPs(uint32_t sessionID,
                         std::shared_ptr<SDPMessageInfo>& localSDP,
                         std::shared_ptr<SDPMessageInfo>& remoteSDP) const
{
  Q_ASSERT(sessionID);
  localSDP = negotiation_.getLocalSDP(sessionID);

  remoteSDP = negotiation_.getRemoteSDP(sessionID);
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
    dialogManager_.startCall(waitingToStart_[transportID].contact,
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
  if (sessionToTransportID_.find(sessionID) != sessionToTransportID_.end())
  {
    quint32 transportID = sessionToTransportID_[sessionID];

    if (transports_.find(transportID) != transports_.end())
    {
      QVariant content;
      if(request.method == SIP_ACK && negotiation_.getState(sessionID)
         == NEG_ANSWER_GENERATED)
      {
        request.message->contentLength = 0;
        printNormal(this, "Adding SDP content to request");

        request.message->contentType = MT_APPLICATION_SDP;

        if (!SDPAnswerToContent(content, sessionID))
        {
          printError(this, "Failed to get SDP answer to request");
          return;
        }
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
      if (response.type == SIP_OK
          && response.message->cSeq.method == SIP_INVITE
          && negotiation_.getState(sessionID) == NEG_NO_STATE)
      {
        printNormal(this, "Adding SDP to an OK response");
        response.message->contentLength = 0;
        response.message->contentType = MT_APPLICATION_SDP;
        if (!SDPOfferToContent(content, transports_[transportID]->getLocalAddress(), sessionID))
        {
          return;
        }
      }
      // if they sent an offer in their INVITE
      else if (negotiation_.getState(sessionID) == NEG_ANSWER_GENERATED)
      {
        printNormal(this, "Adding SDP to response since INVITE had an SDP.");

        response.message->contentLength = 0;
        response.message->contentType = MT_APPLICATION_SDP;
        if (!SDPAnswerToContent(content, sessionID))
        {
          printError(this, "Failed to get SDP answer to response");
          return;
        }
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
  if (dialogManager_.identifySession(request, localAddress, sessionID))
  {
    Q_ASSERT(sessionID);
    if (sessionID != 0)
    {
      sessionToTransportID_[sessionID] = transportID;

      if((request.method == SIP_INVITE || request.method == SIP_ACK)
         && request.message->contentType == MT_APPLICATION_SDP)
      {
        switch (negotiation_.getState(sessionID))
        {
        case NEG_NO_STATE:
        {
          printDebug(DEBUG_NORMAL, this, 
                     "Got first SDP offer.");
          if(!processOfferSDP(sessionID, content, transports_[transportID]->getLocalAddress()))
          {
             printDebug(DEBUG_PROGRAM_ERROR, this, 
                        "Failure to process SDP offer not implemented.");

             //foundDialog->server->setCurrentRequest(request); // TODO
             //sendResponse(sessionID, SIP_DECLINE, request.type);
             return;
          }
          break;
        }
        case NEG_OFFER_GENERATED:
        {
          printDebug(DEBUG_NORMAL, this, 
                     "Got an SDP answer.");
          processAnswerSDP(sessionID, content);
          break;
        }
        case NEG_ANSWER_GENERATED: // TODO: Not sure if these make any sense
        {
          printDebug(DEBUG_NORMAL, this, 
                     "They sent us another SDP offer.");
          processOfferSDP(sessionID, content, transports_[transportID]->getLocalAddress());
          break;
        }
        case NEG_FINISHED:
        {
          printDebug(DEBUG_NORMAL, this, 
                     "Got a new SDP offer in response.");
          processOfferSDP(sessionID, content, transports_[transportID]->getLocalAddress());
          break;
        }
        }
      }
      dialogManager_.processSIPRequest(request, sessionID);
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

  if(!dialogManager_.identifySession(response, sessionID))
  {
    printDebug(DEBUG_PEER_ERROR, this, 
               "Could not identify response session");
    return;
  }

  if(response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    if(response.message->contentType == MT_APPLICATION_SDP)
    {
      if(sessionToTransportID_.find(sessionID) == sessionToTransportID_.end())
      {
        printDebug(DEBUG_WARNING, this, 
                   "Could not identify transport for session");
        return;
      }

      quint32 transportID = sessionToTransportID_[sessionID];

      switch (negotiation_.getState(sessionID))
      {
      case NEG_NO_STATE:
      {
        printDebug(DEBUG_NORMAL, this, 
                   "Got first SDP offer.");
        if(!processOfferSDP(sessionID, content, transports_[transportID]->getLocalAddress()))
        {
           printDebug(DEBUG_PROGRAM_ERROR, this, 
                      "Failure to process SDP offer not implemented.");

           //foundDialog->server->setCurrentRequest(request); // TODO
           //sendResponse(sessionID, SIP_DECLINE, request.type);
           return;
        }
        break;
      }
      case NEG_OFFER_GENERATED:
      {
        printDebug(DEBUG_NORMAL, this, 
                   "Got an SDP answer.");
        processAnswerSDP(sessionID, content);
        break;
      }
      case NEG_ANSWER_GENERATED: // TODO: Not sure if these make any sense
      {
        printDebug(DEBUG_NORMAL, this, 
                   "They sent us another SDP offer.");
        processOfferSDP(sessionID, content, transports_[transportID]->getLocalAddress());
        break;
      }
      case NEG_FINISHED:
      {
        printDebug(DEBUG_NORMAL, this, 
                   "Got a new SDP offer in response.");
        processOfferSDP(sessionID, content, transports_[transportID]->getLocalAddress());
        break;
      }
      }
    }
  }

  dialogManager_.processSIPResponse(response, sessionID);
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


bool SIPManager::SDPOfferToContent(QVariant& content, QString localAddress,
                                   uint32_t sessionID)
{
  std::shared_ptr<SDPMessageInfo> pointer;

  printDebug(DEBUG_NORMAL, this,  "Adding one-to-one SDP.");
  if(!negotiation_.generateOfferSDP(localAddress, sessionID))
  {
    printWarning(this, "Failed to generate local SDP when sending offer.");
    return false;
  }
   pointer = negotiation_.getLocalSDP(sessionID);

  Q_ASSERT(pointer != nullptr);

  SDPMessageInfo sdp = *pointer;
  content.setValue(sdp);
  return true;
}


bool SIPManager::processOfferSDP(uint32_t sessionID, QVariant& content,
                                 QString localAddress)
{
  if(!content.isValid())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, 
                     "The SDP content is not valid at processing. "
                     "Should be detected earlier.");
    return false;
  }

  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if(!negotiation_.generateAnswerSDP(retrieved, localAddress, sessionID))
  {
    printWarning(this, "Remote SDP not suitable or we have no ports to assign");
    negotiation_.endSession(sessionID);
    return false;
  }

  return true;
}


bool SIPManager::SDPAnswerToContent(QVariant &content, uint32_t sessionID)
{
  SDPMessageInfo sdp;
  std::shared_ptr<SDPMessageInfo> pointer = negotiation_.getLocalSDP(sessionID);
  if (pointer == nullptr)
  {
    return false;
  }
  sdp = *pointer;
  content.setValue(sdp);
  return true;
}


bool SIPManager::processAnswerSDP(uint32_t sessionID, QVariant &content)
{
  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if (!content.isValid())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, 
               "Content is not valid when processing SDP. "
               "Should be detected earlier.");
    return false;
  }

  if(!negotiation_.processAnswerSDP(retrieved, sessionID))
  {
    return false;
  }

  return true;
}
