#include "sipmanager.h"

#include "siprouting.h"
#include "sipsession.h"

#include "sipparser.h"

const bool DIRECTMESSAGES = false;


SIPManager::SIPManager():
  isConference_(false),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  localName_(""),
  localUsername_("")
{}

void SIPManager::init()
{
  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveTCPConnection(Connection*)));

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

  sdp_.setPortRange(21500, 22000, 42);
}

void SIPManager::uninit()
{}

QList<uint32_t> SIPManager::startCall(QList<Contact> addresses)
{
  QList<uint32_t> calls;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    SIPDialogData* dialog = new SIPDialogData;

    dialog->con = new Connection(dialogs_.size() + 1, true);

    dialogMutex_.lock();
    dialog->con->setID(dialogs_.size() + 1);
    dialog->session = createSIPSession(dialogs_.size() + 1);
    dialog->routing = new SIPRouting;
    dialog->routing->setLocalNames(localUsername_, localName_);
    dialog->routing->setRemoteUsername(addresses.at(i).username);
    dialog->hostedSession = false;

    //dialog->callID = dialog->session->startCall();

    QObject::connect(dialog->con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processSIPMessage(QString, QString, quint32)));

    QObject::connect(dialog->con, SIGNAL(connected(quint32)),
                     this, SLOT(connectionEstablished(quint32)));

    // message is sent only after connection has been established so we know our address
    dialog->con->establishConnection(addresses.at(i).remoteAddress, sipPort_);

    dialogs_.push_back(dialog);
    calls.push_back(dialogs_.size());
    dialogMutex_.unlock();
    qDebug() << "Added a new dialog:" << dialogs_.size();
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

SIPSession *SIPManager::createSIPSession(uint32_t sessionID)
{
  SIPSession* session = new SIPSession();

  session->init(sessionID);

  QObject::connect(session, SIGNAL(sendRequest(uint32_t,RequestType)),
                   this, SLOT(sendRequest(uint32_t,RequestType)));

  QObject::connect(session, SIGNAL(sendResponse(uint32_t,RequestType)),
                   this, SLOT(sendResponse(uint32_t,RequestType)));

  // connect signals to signals. Session is responsible for responses
  // and callmanager handles informing the user.
  return session;
}

void SIPManager::receiveTCPConnection(Connection* con)
{
  Q_ASSERT(con);
  SIPDialogData* dialog = new SIPDialogData;
  dialog->con = con;
  dialog->session = NULL;

  QObject::connect(dialog->con, SIGNAL(messageAvailable(QString,QString, quint32)),
                   this, SLOT(processSIPMessage(QString, QString, quint32)));
}

// connection has been established. This enables for us to get the needed info
// to form a SIP message
void SIPManager::connectionEstablished(quint32 sessionID)
{
  if(sessionID == 0 || sessionID> dialogs_.size())
  {
    qDebug() << "WARNING: Missing session for connected session";
    return;
  }
  SIPDialogData* dialog = dialogs_.at(sessionID - 1);


  if(dialog->hostedSession)
  {
    qDebug() << "Setting connection as a SIP server connection.";
    dialog->routing->setLocalAddress(dialog->con->getLocalAddress().toString());
    dialog->routing->setLocalHost(dialog->con->getPeerAddress().toString());

    // TODO enable remote to have a different host from ours
    dialog->routing->setRemoteHost(dialog->con->getPeerAddress().toString());
  }
  else
  {
    qDebug() << "Setting connection as a peer-to-peer SIP connection. Firewall needs to be open for this";
    dialog->routing->setLocalAddress(dialog->con->getLocalAddress().toString());
    dialog->routing->setLocalHost(dialog->con->getLocalAddress().toString());
    dialog->routing->setRemoteHost(dialog->con->getPeerAddress().toString());
  }

  qDebug() << "Setting info for peer-to-peer connection";
  // TODO make possible to set the server

  qDebug() << "Connection" << sessionID << "connected."
           << "From:" << dialog->con->getLocalAddress().toString()
           << "To:" << dialog->con->getPeerAddress().toString();

  if(!dialog->session->startCall())
  {
    qDebug() << "WARNING: Could not start a call according to session.";
  }
}

void SIPManager::processSIPRequest(RequestType request, std::shared_ptr<SIPRoutingInfo> routing,
                       std::shared_ptr<SIPSessionInfo> session,
                       std::shared_ptr<SIPMessageInfo> message,
                       quint32 sessionID)
{
  if(!dialogs_.at(sessionID - 1)->routing->incomingSIPRequest(routing))
  {
    qDebug() << "Something wrong with incoming SIP request";
    // TODO: sent to wrong address
    return;
  }
  //dialogs_.at(sessionID - 1)->session->processRequest();
}

void SIPManager::processSIPResponse(ResponseType response, std::shared_ptr<SIPRoutingInfo> routing,
                        std::shared_ptr<SIPSessionInfo> session,
                        std::shared_ptr<SIPMessageInfo> message, quint32 sessionID)
{
  if(!dialogs_.at(sessionID - 1)->routing->incomingSIPResponse(routing))
  {

    qDebug() << "Something wrong with incoming SIP response";
    // TODO: sent to wrong address
    return;
  }
}

void SIPManager::sendRequest(uint32_t sessionID, RequestType request)
{
  qDebug() << "---- Iniated sending of a request ---";
  Q_ASSERT(sessionID < dialogs_.size());
  // get routinginfo for
  QString direct = "";

  // get routing info from  siprouting
  std::shared_ptr<SIPRoutingInfo> routing = dialogs_.at(sessionID - 1)->routing->requestRouting(direct);
  std::shared_ptr<SIPSessionInfo> session = dialogs_.at(sessionID - 1)->session->getSessionInfo();
  SIPMessageInfo mesg_info = dialogs_.at(sessionID - 1)->session->generateMessage(request);

  // convert routingInfo to SIPMesgInfo to struct fields
  // check that all fields are present
  // check message (and maybe parse?)
  // create and attach SDP if necessary
  // send string

  messageID id = messageComposer_.startSIPRequest(request);

  messageComposer_.to(id, routing->receiverRealname, routing->receiverUsername,
                        routing->receiverHost, session->remoteTag);
  messageComposer_.fromIP(id, routing->senderRealname, routing->senderUsername,
                          QHostAddress(routing->senderHost), session->localTag);

  for(QString viaAddress : routing->senderReplyAddress)
  {
    messageComposer_.viaIP(id, QHostAddress(viaAddress), mesg_info.branch);
  }

  messageComposer_.maxForwards(id, routing->maxForwards);
  messageComposer_.setCallID(id, session->callID, routing->senderHost);
  messageComposer_.sequenceNum(id, mesg_info.cSeq, mesg_info.transactionRequest);

  if(request == INVITE)
  {
    sdp_.setLocalInfo(QHostAddress(routing->senderHost), routing->senderUsername);
    std::shared_ptr<SDPMessageInfo> sdp = sdp_.localInviteSDP();
    QString sdp_str = messageComposer_.formSDP(sdp);
    messageComposer_.addSDP(id,sdp_str);
  }

  QString message = messageComposer_.composeMessage(id);

  dialogs_.at(sessionID - 1)->con->sendPacket(message);

  qDebug() << "---- Finished sending of a request ---";
}

void SIPManager::sendResponse(uint32_t sessionID, ResponseType response)
{
  qDebug() << "WARNING: Sending responses not implemented yet";

}

void SIPManager::destroySession(SIPDialogData *dialog)
{
  if(dialog != NULL)
  {
    if(dialog->con != NULL)
    {
      dialog->con->exit(0); // stops qthread
      dialog->con->stopConnection(); // exits run loop
      while(dialog->con->isRunning())
      {
        qSleep(5);
      }
      delete dialog->con;
      dialog->con = NULL;
    }

    if(dialog->session != NULL)
    {
      //dialog->session->uninit();
      delete dialog->session;
      dialog->session = NULL;
    }
  }
}
