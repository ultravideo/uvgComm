#include "sipmanager.h"

#include <QObject>


const quint32 FIRSTTRANSPORTID = 1;

SIPManager::SIPManager():
  tcpServer_(),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  transports_(),
  nextTransportID_(FIRSTTRANSPORTID),
  transactions_()
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
  transactions_.getSDPs(sessionID, localSDP, remoteSDP);
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


void SIPManager::transportRequest(uint32_t sessionID, SIPRequest &request,
                                  QVariant& content)
{
  if (sessionToTransportID_.find(sessionID) != sessionToTransportID_.end())
  {
    quint32 transportID = sessionToTransportID_[sessionID];

    if (transports_.find(transportID) != transports_.end())
    {
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

void SIPManager::transportResponse(uint32_t sessionID, SIPResponse &response,
                                   QVariant& content)
{
  if (sessionToTransportID_.find(sessionID) != sessionToTransportID_.end())
  {
    quint32 transportID = sessionToTransportID_[sessionID];

    if (transports_.find(transportID) != transports_.end())
    {
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
  uint32_t sessionID = 0;
  if (transactions_.identifySession(request, localAddress, sessionID))
  {
    if (sessionID != 0)
    {
      sessionToTransportID_[sessionID] = transportID;
      transactions_.processSIPRequest(request, localAddress, content, sessionID);
    }
    else {
      printDebug(DEBUG_ERROR, metaObject()->className(),
                 DC_RECEIVE_SIP_REQUEST, "transactions did not set new sessionID");
    }
  }
  else {
    if (sessionID != 0)
    {
      transactions_.processSIPRequest(request, localAddress,content, sessionID);
    }
    else {
      printDebug(DEBUG_WARNING, metaObject()->className(),
                 DC_RECEIVE_SIP_REQUEST, "transactions could not identify session");
    }
  }
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
                   &transactions_, &SIPTransactions::processSIPResponse);

  QObject::connect(connection.get(), &SIPTransport::sipTransportEstablished,
                   this, &SIPManager::connectionEstablished);
  transports_[transportID] = connection;

  return connection;
}


bool SIPManager::isConnected(QString remoteAddress, quint32& transportID)
{
  for(auto transport : transports_)
  {
    if(transport != nullptr &&
       transport->getRemoteAddress().toString() == remoteAddress)
    {
      transportID = transport->getTransportID();
      return true;
    }
  }
  return false;
}
