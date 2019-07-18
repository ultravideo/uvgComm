#include "sipmanager.h"

#include <QObject>


const quint32 FIRSTTRANSPORTID = 1;

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transports_(),
  nextTransportID_(FIRSTTRANSPORTID),
  transactions_(),
  negotiation_()
{}


// start listening to incoming
void SIPManager::init(SIPTransactionUser* callControl)
{
  QObject::connect(&tcpServer_, &ConnectionServer::newConnection,
                   this, &SIPManager::receiveTCPConnection);

  tcpServer_.setProxy(QNetworkProxy::NoProxy);

  // listen to everything
  qDebug() << "Initiating," << metaObject()->className()
           << ": Listening to SIP TCP connections on port:" << sipPort_;
  if (!tcpServer_.listen(QHostAddress::Any, sipPort_))
  {
    printDebug(DEBUG_ERROR, this, DC_STARTUP,
               "Failed to listen to socket. Is it reserved?");

    // TODO announce it to user!
  }

  transactions_.init(callControl);

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  QString username = !settings.value("local/Username").isNull()
      ? settings.value("local/Username").toString() : "anonymous";

  negotiation_.setLocalInfo(username);

  QObject::connect(&transactions_, &SIPTransactions::transportRequest,
                   this, &SIPManager::transportRequest);
  QObject::connect(&transactions_, &SIPTransactions::transportResponse,
                   this, &SIPManager::transportResponse);
}


void SIPManager::uninit()
{
  transactions_.uninit();

  for(std::shared_ptr<SIPTransport> transport : transports_)
  {
    if(transport != nullptr)
    {
      transport->cleanup();
      transport.reset();
    }
  }
}


void SIPManager::bindToServer()
{
  // get server address from settings and bind to server.
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  QString serverAddress = settings.value("sip/ServerAddress").toString();

  if (serverAddress != "")
  {
    std::shared_ptr<SIPTransport> transport = createSIPTransport();
    transport->createConnection(TCP, serverAddress);

    sessionToTransportID_[transactions_.reserveSessionID()] = transport->getTransportID();

    waitingToBind_[transport->getTransportID()] = serverAddress;
  }
  else {
    qDebug() << "SIP server address was empty in settings";
  }
}


uint32_t SIPManager::startCall(Contact& address)
{
  quint32 transportID = 0;
  uint32_t sessionID = transactions_.reserveSessionID();

  // There should not exist a dialog for this user
  if(!negotiation_.canStartSession())
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL, "Not enough ports to start a call.");
    return 0;
  }

  // check if we are already connected to remoteaddress and set transportID
  if (!isConnected(address.remoteAddress, transportID))
  {
    // we are not yet connected to them. Form a connection by creating the transport layer
    std::shared_ptr<SIPTransport> transport = createSIPTransport();
    transportID = transport->getTransportID(); // Get new transportID
    sessionToTransportID_[sessionID] = transportID;
    transport->createConnection(TCP, address.remoteAddress);
    waitingToStart_[transportID] = {sessionID, address};
  }
  else {
    // associate this sessionID with existing transportID
    sessionToTransportID_[sessionID] = transportID;
    // we have an existing connection already. Send SIP message and start call.
    transactions_.startDirectCall(address,
                                  transports_[transportID]->getLocalAddress(),
                                  sessionID);
  }

  return sessionID;
}


void SIPManager::acceptCall(uint32_t sessionID)
{

  // Start candiate nomination. This function won't block,
  // negotiation happens in the background remoteFinalSDP()
  // makes sure that a connection was in fact nominated
  negotiation_.startICECandidateNegotiation(sessionID);

  transactions_.acceptCall(sessionID);
}


void SIPManager::rejectCall(uint32_t sessionID)
{
  transactions_.rejectCall(sessionID);
}


void SIPManager::cancelCall(uint32_t sessionID)
{
  transactions_.cancelCall(sessionID);
}


void SIPManager::endCall(uint32_t sessionID)
{
  transactions_.endCall(sessionID);
}


void SIPManager::endAllCalls()
{
  transactions_.endAllCalls();
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
  qDebug() << "Received a TCP connection. Initializing dialog";
  Q_ASSERT(con);

  std::shared_ptr<SIPTransport> transport = createSIPTransport();
  transport->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
}


void SIPManager::connectionEstablished(quint32 transportID)
{
  if (waitingToStart_.find(transportID) != waitingToStart_.end())
  {
    transactions_.startDirectCall(waitingToStart_[transportID].contact,
                                  transports_[transportID]->getLocalAddress(),
                                  waitingToStart_[transportID].sessionID);
  }

  if(waitingToBind_.find(transportID) != waitingToBind_.end())
  {
    transactions_.bindToServer(waitingToBind_[transportID],
                               transports_[transportID]->getLocalAddress(),
                               transportID);
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
      if(request.type == SIP_INVITE)
      {
        request.message->content.length = 0;
        qDebug() << "Adding SDP content to request:" << request.type;
        request.message->content.type = APPLICATION_SDP;

        if (!SDPOfferToContent(content,
                               transports_[transportID]->getLocalAddress(),
                               sessionID))
        {
          return;
        }
      }


      transports_[transportID]->sendRequest(request, content);
    }
    else {
      printDebug(DEBUG_ERROR,  metaObject()->className(), DC_SEND_SIP_REQUEST,
                 "Tried to send request with invalid transportID");
    }
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(), DC_SEND_SIP_REQUEST,
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
      QVariant content;
      if (response.type == SIP_OK
          && response.message->transactionRequest == SIP_INVITE)
      {
        response.message->content.length = 0;
        qDebug() << "Adding SDP content to request:" << response.type;
        response.message->content.type = APPLICATION_SDP;
        if (!SDPAnswerToContent(content, sessionID))
        {
          return;
        }
      }

      transports_[transportID]->sendResponse(response, content);
    }
    else {
      printDebug(DEBUG_ERROR, metaObject()->className(), DC_SEND_SIP_RESPONSE,
                 "Tried to send response with invalid.", {"transportID"},
      {QString::number(transportID)});
    }
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(), DC_SEND_SIP_RESPONSE,
                "No mapping from sessionID to transportID");
  }
}


void SIPManager::processSIPRequest(SIPRequest& request, QHostAddress localAddress,
                                   QVariant& content, quint32 transportID)
{
  if(request.type == SIP_INVITE && !negotiation_.canStartSession())
  {
    printDebug(DEBUG_ERROR, this, DC_RECEIVE_SIP_REQUEST,
               "Got INVITE, but unable to start a call");
    return;
  }

  uint32_t sessionID = 0;

  // sets the sessionID if session exists or creates a new session if INVITE
  // returns true if either was successful.
  if (transactions_.identifySession(request, localAddress, sessionID))
  {
    Q_ASSERT(sessionID);
    if (sessionID != 0)
    {
      sessionToTransportID_[sessionID] = transportID;

      // TODO: prechecks that the message is ok, then modify program state.
      if(request.type == SIP_INVITE)
      {
        if(request.message->content.type == APPLICATION_SDP)
        {
          if(request.type == SIP_INVITE)
          {
            if(!processOfferSDP(sessionID, content, localAddress))
            {
              qDebug() << "Failed to find suitable SDP.";

              // TODO: Announce to transactions that we could not process their SDP
              printDebug(DEBUG_PROGRAM_ERROR, this, DC_RECEIVE_SIP_REQUEST,
                         "Failure to process SDP offer not implemented.");

              //foundDialog->server->setCurrentRequest(request); // TODO
              //sendResponse(sessionID, SIP_DECLINE, request.type);

              return;
            }
          }
        }
        else
        {
          qDebug() << "PEER ERROR: No SDP in" << request.type;
          // TODO: tell SIP transactions that they did not have an SDP
          // sendResponse(sessionID, SIP_DECLINE, request.type);
          return;
        }
      }


      transactions_.processSIPRequest(request, sessionID);
    }
    else
    {
      printDebug(DEBUG_PROGRAM_ERROR, metaObject()->className(),
                 DC_RECEIVE_SIP_REQUEST, "transactions did not set new sessionID.");
    }
  }
  else
  {
    printDebug(DEBUG_PEER_ERROR, metaObject()->className(),
               DC_RECEIVE_SIP_REQUEST, "transactions could not identify session.");
  }
}


void SIPManager::processSIPResponse(SIPResponse &response, QVariant& content)
{
  uint32_t sessionID = 0;

  if(!transactions_.identifySession(response, sessionID))
  {
    printDebug(DEBUG_PEER_ERROR, this, DC_RECEIVE_SIP_RESPONSE,
               "Could not identify response session");
    return;
  }

  if(response.message->transactionRequest == SIP_INVITE && response.type == SIP_OK)
  {
    if(response.message->content.type == APPLICATION_SDP)
    {
      if(!processAnswerSDP(sessionID, content))
      {
        qDebug() << "PEER_ERROR:"
                 << "Their final sdp is not suitable. They should have followed our SDP!!!";
        return;
      }

      negotiation_.setICEPorts(sessionID);
    }
  }

  transactions_.processSIPResponse(response, sessionID);
}


std::shared_ptr<SIPTransport> SIPManager::createSIPTransport()
{
  quint32 transportID = nextTransportID_;
  ++nextTransportID_;

  std::shared_ptr<SIPTransport> connection =
      std::shared_ptr<SIPTransport>(new SIPTransport(transportID));

  QObject::connect(connection.get(), &SIPTransport::incomingSIPRequest,
                   this, &SIPManager::processSIPRequest);

  QObject::connect(connection.get(), &SIPTransport::incomingSIPResponse,
                   this, &SIPManager::processSIPResponse);

  QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                   this, &SIPManager::connectionEstablished);
  transports_[transportID] = connection;

  return connection;
}


bool SIPManager::isConnected(QString remoteAddress, quint32& outTransportID)
{
  for(auto transport : transports_)
  {
    if(transport != nullptr &&
       transport->getRemoteAddress().toString() == remoteAddress)
    {
      outTransportID = transport->getTransportID();
      return true;
    }
  }
  return false;
}


bool SIPManager::SDPOfferToContent(QVariant& content, QHostAddress localAddress,
                                   uint32_t sessionID)
{
  SDPMessageInfo sdp;

  if(!negotiation_.generateOfferSDP(localAddress, sessionID))
  {
    qDebug() << "Failed to generate SDP Suggestion while sending: "
                "Possibly because we ran out of ports to assign";
    return false;
  }

  std::shared_ptr<SDPMessageInfo> pointer = negotiation_.getLocalSDP(sessionID);
  sdp = *pointer;
  content.setValue(sdp);
  return true;
}


bool SIPManager::processOfferSDP(uint32_t sessionID, QVariant& content,
                                 QHostAddress localAddress)
{
  if(!content.isValid())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SIP_CONTENT,
                     "The SDP content is not valid at processing. "
                     "Should be detected earlier.");
    return false;
  }

  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if(!negotiation_.generateAnswerSDP(retrieved, localAddress, sessionID))
  {
    qDebug() << "Remote SDP not suitable or we have no ports to assign";
    negotiation_.endSession(sessionID);
    return false;
  }
  return true;
}


bool SIPManager::SDPAnswerToContent(QVariant &content, uint32_t sessionID)
{
  SDPMessageInfo sdp;
  std::shared_ptr<SDPMessageInfo> pointer = negotiation_.getLocalSDP(sessionID);
  sdp = *pointer;
  content.setValue(sdp);
  return true;
}


bool SIPManager::processAnswerSDP(uint32_t sessionID, QVariant &content)
{
  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if (!content.isValid())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_NEGOTIATING,
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
