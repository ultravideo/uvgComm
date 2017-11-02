#include "sipmanager.h"

SIPManager::SIPManager():
  sipPort_(5060) // default for SIP, use 5061 for tls encrypted
{}

void SIPManager::init()
{
  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveTCPConnection(Connection*)));

  // listen to everything
  // TODO: maybe record the incoming connection address and choose used network interface address
  // according to that
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);
}

void SIPManager::uninit()
{
  //state_.uninit();
}

QList<QString> SIPManager::startCall(QList<Contact> addresses)
{
  QList<QString> callIDs;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    SIPSession* session = new SIPSession;
    session->con = new Connection(sessions_.size() + 1, true);
    session->con->setID(sessions_.size() + 1);
    session->state = createSIPState();


    session->callID = session->state->startCall(addresses.at(i));
    callIDs.push_back(session->callID);

    QObject::connect(session->con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processSIPMessage(QString, QString, quint32)));

    QObject::connect(session->con, SIGNAL(connected(quint32)),
                     this, SLOT(connectionEstablished(quint32)));

    session->con->establishConnection(addresses.at(i).contactAddress, sipPort_);

    sessionMutex_.lock();
    sessions_.push_back(session);
    sessionMutex_.unlock();
    qDebug() << "Added a new session:" << sessions_.size();
  }

  return callIDs;
}

void SIPManager::acceptCall(QString callID)
{
  //state_.acceptCall(callID);
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
void SIPManager::connectionEstablished(quint32 connectionID)
{
  qDebug() << "Connection" << connectionID << "connected.";
}


void SIPManager::processSIPMessage(QString header, QString content, quint32 connectionID)
{
  qDebug() << "Connection " << connectionID << "received SIP message. Processing...";



}
