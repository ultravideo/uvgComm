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

    waitingToBind_[transport->getTransportID()] = serverAddress;
  }
  else {
    qDebug() << "SIP server address was empty in settings";
  }
}


uint32_t SIPManager::startCall(Contact& address)
{
  quint32 transportID = 0;

  // TODO: should first check if we have a server connection already.
  if (!isConnected(address.remoteAddress, transportID))
  {
    std::shared_ptr<SIPTransport> transport = createSIPTransport();
    transport->createConnection(TCP, address.remoteAddress);
    transportID = transport->getTransportID();

    waitingToStart_[transportID] = address;
  }
  else {
    // we have an existing connection already. Send SIP message and start call.
    return transactions_.startDirectCall(address,
                                         transports_[transportID]->getLocalAddress(),
                                         transportID);
  }

  return 0;
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
             std::shared_ptr<SDPMessageInfo>& remoteSDP)
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
    transactions_.startDirectCall(waitingToStart_[transportID],
                                  transports_[transportID]->getLocalAddress(),
                                  transportID);
  }

  if(waitingToBind_.find(transportID) != waitingToBind_.end())
  {
    transactions_.bindToServer(waitingToBind_[transportID],
                               transports_[transportID]->getLocalAddress(),
                               transportID);
  }
}


void SIPManager::transportRequest(quint32 transportID, SIPRequest &request, QVariant& content)
{
  if (transports_.find(transportID) != transports_.end())
  {
    transports_[transportID]->sendRequest(request, content);
  }
  else {
    printDebug(DEBUG_ERROR,  metaObject()->className(), DC_SEND_SIP_REQUEST,
               "Tried to send request with invalid transportID");
  }
}

void SIPManager::transportResponse(quint32 transportID, SIPResponse &response, QVariant& content)
{
  if (transports_.find(transportID) != transports_.end())
  {
    transports_[transportID]->sendResponse(response, content);
  }
  else {
    printDebug(DEBUG_ERROR, metaObject()->className(), DC_SEND_SIP_RESPONSE,
               "Tried to send response with invalid.", {"transportID"}, {QString::number(transportID)});
  }
}


std::shared_ptr<SIPTransport> SIPManager::createSIPTransport()
{
  quint32 transportID = nextTransportID_;
  ++nextTransportID_;

  std::shared_ptr<SIPTransport> connection =
      std::shared_ptr<SIPTransport>(new SIPTransport(transportID));

  QObject::connect(connection.get(), &SIPTransport::incomingSIPRequest,
                   &transactions_, &SIPTransactions::processSIPRequest);
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
