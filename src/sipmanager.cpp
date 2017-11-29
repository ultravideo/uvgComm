#include "sipmanager.h"

#include "siprouting.h"
#include "sipsession.h"

const bool DIRECTMESSAGES = false;


SIPManager::SIPManager():
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
}

void SIPManager::uninit()
{}

QList<QString> SIPManager::startCall(QList<Contact> addresses)
{
  QList<QString> callIDs;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {

    SIPDialogData* dialog = new SIPDialogData;

    dialog->con = new Connection(dialogs_.size() + 1, true);
    dialog->con->setID(dialogs_.size() + 1);

    dialog->session = createSIPSession();

    dialog->routing = new SIPRouting;
    dialog->hostedSession = false;

    //dialog->callID = dialog->session->startCall();
    callIDs.push_back(dialog->callID);

    QObject::connect(dialog->con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processSIPMessage(QString, QString, quint32)));

    QObject::connect(dialog->con, SIGNAL(connected(quint32)),
                     this, SLOT(connectionEstablished(quint32)));

    // message is sent only after connection has been established so we know our address
    dialog->con->establishConnection(addresses.at(i).remoteAddress, sipPort_);

    dialogMutex_.lock();
    dialogs_.push_back(dialog);
    dialogMutex_.unlock();
    qDebug() << "Added a new dialog:" << dialogs_.size();
  }

  return callIDs;
}

void SIPManager::acceptCall(QString callID)
{

}

void SIPManager::rejectCall(QString callID)
{
  //state_.rejectCall(callID);
}

void SIPManager::endCall(QString callID)
{
  //state_.endCall(callID);
}

void SIPManager::endAllCalls()
{
  //state_.endAllCalls();
}

SIPSession *SIPManager::createSIPSession()
{
  SIPSession* session = new SIPSession();

  QObject::connect(session, SIGNAL(sendRequest(uint32_t,RequestType,SIPSessionInfo)),
                   this, SLOT(sendRequest(uint32_t,RequestType,SIPSessionInfo)));

  /*
  session->init();

  // would use newer syntax if it supported qstring
  QObject::connect(state, SIGNAL(incomingINVITE(QString, QString)), this, SIGNAL(incomingINVITE(QString, QString)));
  QObject::connect(state, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)),
                   this, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)));
  QObject::connect(state, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)),
                   this, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)));
  QObject::connect(state, SIGNAL(ringing(QString)), this, SIGNAL(ringing(QString)));
  QObject::connect(state, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)),
                   this, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)));
  QObject::connect(state, SIGNAL(ourCallRejected(QString)), this, SIGNAL(ourCallRejected(QString)));
  QObject::connect(state, SIGNAL(callEnded(QString, QString)), this, SIGNAL(callEnded(QString, QString)));
  return state; */


}

void SIPManager::receiveTCPConnection(Connection* con)
{
  Q_ASSERT(con);
  SIPDialogData* dialog = new SIPDialogData;
  dialog->con = con;
  dialog->session = NULL;
  dialog->callID = "";

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
    // TODO: get server
    qDebug() << "Forming connection through SIP server";

    dialog->routing->initLocal(localUsername_,
                                dialog->con->getLocalAddress().toString(),
                                dialog->con->getLocalAddress().toString(),
                                dialog->con->getLocalAddress().toString());
  }
  else
  {
    qDebug() << "Forming a peer-to-peer SIP connection. Firewall needs to be open for this";

    dialog->routing->initLocal(localUsername_,
                                dialog->con->getLocalAddress().toString(),
                                dialog->con->getLocalAddress().toString(),
                                dialog->con->getLocalAddress().toString());
  }
  qDebug() << "Connection" << sessionID << "connected."
           << "From:" << dialog->con->getLocalAddress().toString()
           << "To:" << dialog->con->getPeerAddress().toString();

  // generate SDP
  // sendRequest(INVITE);
}

void SIPManager::processSIPMessage(QString header, QString content, quint32 sessionID)
{
  qDebug() << "Connection " << sessionID << "received SIP message. Processing...";

  // parse to struct of fields
  // check if all fields are present
  // convert struct to routingInfo and SIPMesgInfo
  // check validity of both
  // check if callID matches
  // check if we are the intended destination and sender is who it should be
  // if invite or response to invite, check SDP
  // compare against state and update
  // inform user if necessary
  // respond
}

void SIPManager::sendRequest(uint32_t dialogID_, RequestType request, const SIPSessionInfo &session)
{
  Q_ASSERT(dialogID_ < dialogs_.size());
  // get routinginfo for
  QString direct = "";
  std::shared_ptr<SIPRoutingInfo> routing = dialogs_.at(dialogID_ - 1)->routing->requestRouting(direct);

  // get info from state
  // check validity of routingInfo and SIPMesgInfo
  // convert routingInfo and SIPMesgInfo to struct fields
  // check that all fields are present
  // check message (and maybe parse?)
  // create and attach SDP if necessary
  // send string

  /* messageComposer_.to(id, info->contact.name, info->contact.username,
                        info->contact.contactAddress, info->remoteTag);
  messageComposer_.fromIP(id, localName_, localUsername_, info->localAddress, info->localTag);
  QString branch = generateRandomString(BRANCHLENGTH);
  messageComposer_.viaIP(id, info->localAddress, branch);
  messageComposer_.maxForwards(id, MAXFORWARDS);
  messageComposer_.setCallID(id, info->callID, info->host);
  messageComposer_.sequenceNum(id, 1, info->originalRequest);

  messageComposer_.composeMessage(id);
*/
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
