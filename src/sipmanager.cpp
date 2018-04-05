#include "sipmanager.h"

#include "siprouting.h"
#include "sipsession.h"
#include "sipconnection.h"

#include "sipclienttransaction.h"
#include "sipservertransaction.h"

const bool DIRECTMESSAGES = false;

SIPManager::SIPManager():
  isConference_(false),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  localName_(""),
  localUsername_("")
{}

void SIPManager::init(SIPTransactionUser *callControl)
{
  qDebug() << "Iniating SIP Manager";

  transactionUser_ = callControl;

  QObject::connect(&server_, &ConnectionServer::newConnection,
                   this, &SIPManager::receiveTCPConnection);

  // listen to everything
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);

  QSettings settings;
  QString localName = settings.value("local/Name").toString();
  QString localUsername = settings.value("local/Username").toString();

  if(!localName.isEmpty())
  {
    localName_ = localName;
  }
  else
  {
    localName_ = "Anonymous";
  }

  if(!localUsername.isEmpty())
  {
    localUsername_ = localUsername;
  }
  else
  {
    localUsername_ = "anonymous";
  }
  sdp_.setLocalInfo(QHostAddress("0.0.0.0"), localUsername_);
  sdp_.setPortRange(21500, 22000, 42);
}

void SIPManager::uninit()
{
  //TODO: delete all dialogs
  for(std::shared_ptr<SIPDialogData> dialog : dialogs_)
  {
    destroyDialog(dialog);
  }
}

QList<uint32_t> SIPManager::startCall(QList<Contact> addresses)
{
  QList<uint32_t> calls;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    std::shared_ptr<SIPDialogData> dialog;
    createDialog(dialog);
    dialog->session->setOurSession(true);
    calls.push_back(dialogs_.size());
    dialog->remoteUsername = addresses.at(i).username;
    // message is sent only after connection has been established so we know our address
    dialog->sCon->createConnection(TCP, addresses.at(i).remoteAddress);

    qDebug() << "Added a new dialog. ID:" << dialogs_.size();

    // this start call will commence once the connection has been established
    if(!dialog->client->startCall())
    {
      qDebug() << "WARNING: Could not start a call according to session.";
    }
  }

  return calls;
}

void SIPManager::createDialog(std::shared_ptr<SIPDialogData>& dialog)
{
  dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);
  dialog->sCon = createSIPConnection();
  dialog->remoteUsername = "";
  connectionMutex_.lock();
  dialog->session = createSIPSession(dialogs_.size() + 1);
  dialog->routing = NULL;
  dialog->client = std::shared_ptr<SIPClientTransaction> (new SIPClientTransaction);
  dialog->client->init(transactionUser_, dialogs_.size() + 1);
  dialog->server = std::shared_ptr<SIPServerTransaction> (new SIPServerTransaction);
  dialog->server->init(transactionUser_, dialogs_.size() + 1);

  QObject::connect(dialog->client.get(), &SIPClientTransaction::sendRequest,
                   this, &SIPManager::sendRequest);

  QObject::connect(dialog->server.get(), &SIPServerTransaction::sendResponse,
                   this, &SIPManager::sendResponse);

  dialogs_.push_back(dialog);
  connectionMutex_.unlock();
}

void SIPManager::acceptCall(uint32_t sessionID)
{
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();
  dialog->server->acceptCall();
}

void SIPManager::rejectCall(uint32_t sessionID)
{
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();
  dialog->server->rejectCall();
}

void SIPManager::endCall(uint32_t sessionID)
{
  qDebug() << "WARNING: Not implemented in SIPManager";
}

void SIPManager::endAllCalls()
{
  qDebug() << "WARNING: Not implemented in SIPManager";
}

std::shared_ptr<SIPSession> SIPManager::createSIPSession(uint32_t sessionID)
{
  qDebug() << "Creating SIP Session";
  std::shared_ptr<SIPSession> session = std::shared_ptr<SIPSession> (new SIPSession());
  session->init(sessionID);

  // connect signals to signals. Session is responsible for responses
  // and callmanager handles informing the user.
  return session;
}

std::shared_ptr<SIPConnection> SIPManager::createSIPConnection()
{
  qDebug() << "Creating SIP Connection";
  std::shared_ptr<SIPConnection> connection =
      std::shared_ptr<SIPConnection>(new SIPConnection(dialogs_.size() + 1));
  QObject::connect(connection.get(), &SIPConnection::sipConnectionEstablished,
                   this, &SIPManager::connectionEstablished);
  QObject::connect(connection.get(), &SIPConnection::incomingSIPRequest,
                   this, &SIPManager::processSIPRequest);
  QObject::connect(connection.get(), &SIPConnection::incomingSIPResponse,
                   this, &SIPManager::processSIPResponse);
  return connection;
}

std::shared_ptr<SIPRouting> SIPManager::createSIPRouting(QString remoteUsername,
                                                         QString localAddress,
                                                         QString remoteAddress, bool hostedSession)
{
  qDebug() << "Creating SIP Routing";
  std::shared_ptr<SIPRouting> routing = std::shared_ptr<SIPRouting> (new SIPRouting);

  routing->setLocalNames(localUsername_, localName_);
  routing->setRemoteUsername(remoteUsername);

  if(hostedSession)
  {
    qDebug() << "Setting connection as a SIP server connection.";
    routing->setLocalAddress(localAddress);
    routing->setLocalHost(remoteAddress);

    // TODO enable remote to have a different host from ours
    routing->setRemoteHost(remoteAddress);
  }
  else
  {
    qDebug() << "Setting connection as a peer-to-peer SIP connection. Firewall needs to be open for this";
    routing->setLocalAddress(localAddress);
    routing->setLocalHost(localAddress);
    routing->setRemoteHost(remoteAddress);
  }

  // TODO make possible to set the server

  return routing;
}

void SIPManager::receiveTCPConnection(TCPConnection *con)
{
  // TODO: this could also be for one of the existing sessions, not just a new session

  qDebug() << "Received a TCP connection. Initializing dialog";
  Q_ASSERT(con);

  std::shared_ptr<SIPDialogData> dialog;
  createDialog(dialog);
  dialog->sCon->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
  qDebug() << "Dialog with ID:" << dialogs_.size() << "created for received connection.";
}

// connection has been established. This enables for us to get the needed info
// to form a SIP message
void SIPManager::connectionEstablished(quint32 sessionID, QString localAddress, QString remoteAddress)
{
  Q_ASSERT(sessionID != 0 || dialogs_.at(sessionID - 1));
  qDebug() << "Establishing connection";
  if(sessionID == 0 || sessionID > dialogs_.size() || dialogs_.at(sessionID - 1) == NULL)
  {
    qDebug() << "WARNING: Missing session for connected session";
    return;
  }
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock(); 

  if(dialog->routing == NULL)
  {
    dialog->routing = createSIPRouting(dialog->remoteUsername, localAddress,
                                       remoteAddress, false);
  }
  if(dialog->session == NULL)
  {
    dialog->session = createSIPSession(sessionID);
  }
  dialog->session->generateCallID(localAddress);

  dialog->client->connectionReady(true);

  qDebug() << "Connection" << sessionID << "connected and dialog created."
           << "From:" << localAddress
           << "To:" << remoteAddress;
}

void SIPManager::processSIPRequest(SIPRequest request,
                       quint32 sessionID, QVariant content)
{
  Q_ASSERT(dialogs_.at(sessionID - 1)->routing);
  Q_ASSERT(dialogs_.at(sessionID - 1)->session);

  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();

  // TODO: prechecks that the message is ok, then process it.

  if(request.type == INVITE)
  {
    if(request.message->content.type == APPLICATION_SDP)
    {
      SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
      dialog->localFinalSdp_ = sdp_.localFinalSDP(retrieved);
    }
    else
    {
      qDebug() << "No SDP in INVITE!";
      sendResponse(sessionID, SIP_DECLINE, request.type);
      return;
    }
  }

  if(!dialog->routing->incomingSIPRequest(request.message->routing))
  {
    qDebug() << "Something wrong with incoming SIP request routing";
    sendResponse(sessionID, SIP_NOT_FOUND, request.type);
    return;
  }

  if(!dialog->session->processRequest(request.message->session))
  {
    qDebug() << "Received a request for a wrong session!";
    sendResponse(sessionID, SIP_CALL_DOES_NOT_EXIST, request.type);
    return;
  }

  dialog->server->processRequest(request);
}

void SIPManager::processSIPResponse(SIPResponse response,
                                    quint32 sessionID, QVariant content)
{
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();

  // TODO: if request was INVITE and response is 2xx or 101-199, create dialog

  if(response.message->transactionRequest == INVITE && response.type == SIP_OK)
  {
    if(response.message->content.type == APPLICATION_SDP)
    {
      SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
      if(!sdp_.remoteFinalSDP(retrieved))
      {
        qDebug() << "OK SDP not suitable.";
      }
      else
      {
        *dialog->remoteFinalSdp_.get() = retrieved;
      }
    }
  }

  if(!dialogs_.at(sessionID - 1)->routing->incomingSIPResponse(response.message->routing))
  {

    qDebug() << "Something wrong with incoming SIP response routing for session:" << sessionID;
    // TODO: sent to wrong address (404 response)
    return;
  }
  qWarning() << "WARNING: Processing responses not implemented yet";

  dialogs_.at(sessionID - 1)->client->processResponse(response);
}

void SIPManager::sendRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a request ---";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  QString directRouting = "";

  // Get all the necessary information from different components.
  SIPRequest request = {type, dialogs_.at(sessionID - 1)->session->getRequestInfo(type)};
  request.message->routing = dialogs_.at(sessionID - 1)->routing->requestRouting(directRouting);

  QVariant content;
  if(type == INVITE)
  {
    request.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp = *sdp_.localSDPSuggestion().get();
    content.setValue(sdp);
  }

  dialogs_.at(sessionID - 1)->sCon->sendRequest(request, content);
  qDebug() << "---- Finished sending of a request ---";
}

void SIPManager::sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest)
{
  qDebug() << "---- Iniated sending of a response ---";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  QString directRouting = "";

  // Get all the necessary information from different components.
  SIPResponse response = {type, dialogs_.at(sessionID - 1)->session->getResponseInfo(originalRequest)};
  response.message->routing = dialogs_.at(sessionID - 1)->routing->requestRouting(directRouting);

  QVariant content;
  if(response.message->transactionRequest == INVITE && type == SIP_OK) // TODO: SDP in progress...
  {
    response.message->content.type = APPLICATION_SDP;
    SDPMessageInfo sdp = *dialogs_.at(sessionID - 1)->localFinalSdp_.get();
    content.setValue(sdp);
  }

  dialogs_.at(sessionID - 1)->sCon->sendResponse(response, content);
  qDebug() << "---- Finished sending of a response ---";
}

void SIPManager::destroyDialog(std::shared_ptr<SIPDialogData> dialog)
{
  if(dialog != NULL)
  {
    if(dialog->sCon != NULL)
    {
      dialog->sCon->destroyConnection();
      dialog->sCon.reset();
    }

    dialog->session.reset();
    dialog->routing.reset();
  }
}
