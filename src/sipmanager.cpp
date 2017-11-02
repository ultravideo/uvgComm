#include "sipmanager.h"

SIPManager::SIPManager()
{

}

void SIPManager::init()
{
  //state_.init();


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

    session->state = new SIPState();
    session->state->init();

    // would use newer syntax if it supported qstring
    QObject::connect(session->state, SIGNAL(incomingINVITE(QString, QString)), this, SIGNAL(incomingINVITE(QString, QString)));
    QObject::connect(session->state, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)),
                     this, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)));
    QObject::connect(session->state, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)),
                     this, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)));
    QObject::connect(session->state, SIGNAL(ringing(QString)), this, SIGNAL(ringing(QString)));
    QObject::connect(session->state, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)),
                     this, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)));
    QObject::connect(session->state, SIGNAL(ourCallRejected(QString)), this, SIGNAL(ourCallRejected(QString)));
    QObject::connect(session->state, SIGNAL(callEnded(QString, QString)), this, SIGNAL(callEnded(QString, QString)));

    session->callID = session->state->startCall(addresses.at(i));
    callIDs.push_back(session->callID);

    QObject::connect(session->con, SIGNAL(messageAvailable(QString, QString, quint32)),
                     this, SLOT(processSIPMessage(QString, QString, quint32)));

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

void SIPManager::processSIPMessage(QString header, QString content, quint32 connectionID)
{
  qDebug() << "Connection " << connectionID << "received SIP message. Processing...";



}
