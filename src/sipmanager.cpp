#include "sipmanager.h"

#include "siprouting.h"
#include "sipsession.h"
#include "sipconnection.h"

const bool DIRECTMESSAGES = false;

SIPManager::SIPManager():
  isConference_(false),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  localName_(""),
  localUsername_("")
{}

void SIPManager::init(CallControlInterface *callControl)
{
  qDebug() << "Iniating SIP Manager";

  callControl_ = callControl;

  QObject::connect(&server_, SIGNAL(newConnection(TCPConnection*)),
                   this, SLOT(receiveTCPConnection(TCPConnection*)));

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
    std::shared_ptr<SIPDialogData> dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);

    dialog->sCon = createSIPConnection();
    dialog->session = createSIPSession(dialogs_.size() + 1);
    dialog->routing = NULL;
    dialog->remoteUsername = addresses.at(i).username;
    connectionMutex_.lock();
    // message is sent only after connection has been established so we know our address
    dialog->sCon->createConnection(TCP, addresses.at(i).remoteAddress);

    dialogs_.push_back(dialog);
    connectionMutex_.unlock();
    calls.push_back(dialogs_.size());
    qDebug() << "Added a new dialog. ID:" << dialogs_.size();

    // this start call will commence once the connection has been established
    if(!dialog->session->startCall())
    {
      qDebug() << "WARNING: Could not start a call according to session.";
    }
  }

  return calls;
}

void SIPManager::acceptCall(uint32_t sessionID)
{

}

void SIPManager::rejectCall(uint32_t sessionID)
{

}

void SIPManager::endCall(uint32_t sessionID)
{

}

void SIPManager::endAllCalls()
{

}

std::shared_ptr<SIPSession> SIPManager::createSIPSession(uint32_t sessionID)
{
  qDebug() << "Creating SIP Session";
  std::shared_ptr<SIPSession> session = std::shared_ptr<SIPSession> (new SIPSession());

  session->init(sessionID, callControl_);

  QObject::connect(session.get(), SIGNAL(sendRequest(uint32_t,RequestType)),
                   this, SLOT(sendRequest(uint32_t,RequestType)));

  QObject::connect(session.get(), SIGNAL(sendResponse(uint32_t,ResponseType)),
                   this, SLOT(sendResponse(uint32_t,ResponseType)));

  // connect signals to signals. Session is responsible for responses
  // and callmanager handles informing the user.
  return session;
}

std::shared_ptr<SIPConnection> SIPManager::createSIPConnection()
{
  qDebug() << "Creating SIP Connection";
  std::shared_ptr<SIPConnection> connection = std::shared_ptr<SIPConnection>(new SIPConnection(dialogs_.size() + 1));
  QObject::connect(connection.get(), SIGNAL(sipConnectionEstablished(quint32,QString,QString)),
                   this, SLOT(connectionEstablished(quint32,QString,QString)));
  QObject::connect(connection.get(), SIGNAL(incomingSIPRequest(SIPRequest,quint32)),
                   this, SLOT(processSIPRequest(SIPRequest,quint32)));
  QObject::connect(connection.get(), SIGNAL(incomingSIPResponse(SIPResponse,quint32)),
                   this, SLOT(processSIPResponse(SIPResponse,quint32)));
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
  std::shared_ptr<SIPDialogData> dialog = std::shared_ptr<SIPDialogData> (new SIPDialogData);
  dialog->sCon = createSIPConnection();
  connectionMutex_.lock();
  dialog->sCon->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
  dialog->session = createSIPSession(dialogs_.size() + 1);
  dialog->routing = NULL;
  dialog->remoteUsername = "";
  dialogs_.append(dialog);
  connectionMutex_.unlock();
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

  if(dialog->session == NULL)
  {
    dialog->session = createSIPSession(sessionID);
  }
  if(dialog->routing == NULL)
  {
    dialog->routing = createSIPRouting(dialog->remoteUsername, localAddress,
                                       remoteAddress, false);
  }

  dialog->session->connectionReady(true);

  qDebug() << "Connection" << sessionID << "connected and dialog created."
           << "From:" << localAddress
           << "To:" << remoteAddress;
}

void SIPManager::processSIPRequest(SIPRequest request,
                       quint32 sessionID)
{
  Q_ASSERT(dialogs_.at(sessionID - 1)->routing);
  Q_ASSERT(dialogs_.at(sessionID - 1)->session);

  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();

  if(!dialog->routing->incomingSIPRequest(request.message->routing))
  {
    qDebug() << "Something wrong with incoming SIP request routing";
    sendResponse(sessionID, SIP_NOT_FOUND);
    return;
  }

  if(!dialog->session->correctRequest(request.message->session))
  {
    qDebug() << "Received a request for a wrong session!";
    sendResponse(sessionID, SIP_CALL_DOES_NOT_EXIST);
    return;
  }

  dialogs_.at(sessionID - 1)->session->processRequest(request);
}

void SIPManager::processSIPResponse(SIPResponse response,
                                    quint32 sessionID)
{
  connectionMutex_.lock();
  std::shared_ptr<SIPDialogData> dialog = dialogs_.at(sessionID - 1);
  connectionMutex_.unlock();

  if(!dialogs_.at(sessionID - 1)->routing->incomingSIPResponse(response.message->routing))
  {

    qDebug() << "Something wrong with incoming SIP response";
    // TODO: sent to wrong address (404 response)
    return;
  }
  qWarning() << "WARNING: Processing responses not implemented yet";
}

void SIPManager::sendRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a request ---";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  QString directRouting = "";

  // Get all the necessary information from different components.
  SIPRequest request = {type, dialogs_.at(sessionID - 1)->session->getRequestInfo(type)};
  request.message->routing = dialogs_.at(sessionID - 1)->routing->requestRouting(directRouting);

  std::shared_ptr<SDPMessageInfo> sdp_info = NULL;
  if(type == INVITE) // TODO: SDP in progress...
  {
    sdp_info = sdp_.localSDPSuggestion();
  }

  dialogs_.at(sessionID - 1)->sCon->sendRequest(request, sdp_info);
  qDebug() << "---- Finished sending of a request ---";
}

void SIPManager::sendResponse(uint32_t sessionID, ResponseType type)
{
  qDebug() << "---- Iniated sending of a response ---";
  Q_ASSERT(sessionID != 0 && sessionID <= dialogs_.size());
  QString directRouting = "";

  // Get all the necessary information from different components.
  SIPResponse response = {type, dialogs_.at(sessionID - 1)->session->getResponseInfo()};
  response.message->routing = dialogs_.at(sessionID - 1)->routing->requestRouting(directRouting);

  std::shared_ptr<SDPMessageInfo> sdp_info = NULL;
  if(response.message->transactionRequest == INVITE) // TODO: SDP in progress...
  {
    sdp_info = dialogs_.at(sessionID - 1)->localFinalSdp_;
    Q_ASSERT(sdp_info);
  }

  dialogs_.at(sessionID - 1)->sCon->sendResponse(response, sdp_info);
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
