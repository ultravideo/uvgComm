#include "sipmanager.h"

SIPManager::SIPManager():
  sipPort_(5060) // default for SIP, use 5061 for tls encrypted
{}

void SIPManager::init()
{
  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveTCPConnection(Connection*)));

  // listen to everything
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);
}

void SIPManager::uninit()
{}

QList<QString> SIPManager::startCall(QList<Contact> addresses)
{
  QList<QString> callIDs;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    SIPSession* session = new SIPSession;
    session->con = new Connection(sessions_.size() + 1, true);
    session->con->setID(sessions_.size() + 1);
    session->state = createSIPState();
    session->hostedSession = false;

    session->callID = session->state->startCall();
    callIDs.push_back(session->callID);

    QObject::connect(session->con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processSIPMessage(QString, QString, quint32)));

    QObject::connect(session->con, SIGNAL(connected(quint32)),
                     this, SLOT(connectionEstablished(quint32)));


    // message is sent only after connection has been established so we know our address
    session->con->establishConnection(addresses.at(i).remoteAddress, sipPort_);

    sessionMutex_.lock();
    sessions_.push_back(session);
    sessionMutex_.unlock();
    qDebug() << "Added a new session:" << sessions_.size();
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

SIPState* SIPManager::createSIPState()
{
  SIPState* state = new SIPState();
  state->init();

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
  return state;
}

void SIPManager::receiveTCPConnection(Connection* con)
{
  SIPSession* session = new SIPSession;
  session->con = con;
  session->state = NULL;
  session->callID = "";

  QObject::connect(con, SIGNAL(messageAvailable(QString,QString, quint32)),
                   this, SLOT(processSIPMessage(QString, QString, quint32)));
}

// connection has been established. This enables for us to get the needed info
// to form a SIP message
void SIPManager::connectionEstablished(quint32 sessionID)
{
  if(sessionID == 0 || sessionID> sessions_.size())
  {
    qDebug() << "WARNING: Missing session for connected session";
  }
  SIPSession* session = sessions_.at(sessionID - 1);
  if(session->hostedSession)
  {
    session->state->setPeerConnection(session->con->getLocalAddress().toString(),
                                      session->con->getPeerAddress().toString());
  }
  else
  {
    session->state->setServerConnection("");
  }
  qDebug() << "Connection" << sessionID << "connected."
           << "From:" << session->con->getLocalAddress().toString()
           << "To:" << session->con->getPeerAddress().toString();

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

void SIPManager::sendRequest()
{
  // get routinginfo for
  // get info from state
  // check validity of routingInfo and SIPMesgInfo
  // convert routingInfo and SIPMesgInfo to struct fields
  // check that all fields are present
  // check message (and maybe parse?)
  // create and attach SDP if necessary
  // send string
}

void SIPManager::destroySession(SIPSession* session)
{
  if(session != NULL)
  {
    if(session->con != NULL)
    {
      session->con->exit(0); // stops qthread
      session->con->stopConnection(); // exits run loop
      while(session->con->isRunning())
      {
        qSleep(5);
      }
      delete session->con;
      session->con = NULL;
    }

    if(session->state != NULL)
    {
      session->state->uninit();
      delete session->state;
      session->state = NULL;
    }
  }
}
