#include "sipmanager.h"

SIPManager::SIPManager()
{

}


void SIPManager::init()
{
  state_.init();

  // would use newer syntax if it supported qstring
  QObject::connect(&state_, SIGNAL(incomingINVITE(QString, QString)), this, SIGNAL(incomingINVITE(QString, QString)));
  QObject::connect(&state_, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)),
                   this, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)));
  QObject::connect(&state_, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)),
                   this, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)));
  QObject::connect(&state_, SIGNAL(ringing(QString)), this, SIGNAL(ringing(QString)));
  QObject::connect(&state_, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)),
                   this, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>, std::shared_ptr<SDPMessageInfo>)));
  QObject::connect(&state_, SIGNAL(ourCallRejected(QString)), this, SIGNAL(ourCallRejected(QString)));
  QObject::connect(&state_, SIGNAL(callEnded(QString, QString)), this, SIGNAL(callEnded(QString, QString)));
}
void SIPManager::uninit()
{
  state_.uninit();
}

QList<QString> SIPManager::startCall(QList<Contact> addresses)
{
  QList<QString> callIDs;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    SIPSession* session = new SIPSession;
    session->con = new Connection(sessions_.size() + 1, true);
    session->state = new SIPState();
    //sessions->callID = session->state->???
    //callIDs.push_back(sessions->callID);

    sessionMutex_.lock();
    sessions_.push_back(session);
    sessionMutex_.unlock();
    qDebug() << "Added a new session:" << sessions_.size();
  }

  return state_.startCall(addresses);
}

void SIPManager::acceptCall(QString callID)
{
  state_.acceptCall(callID);
}

void SIPManager::rejectCall(QString callID)
{
  state_.rejectCall(callID);
}

void SIPManager::endCall(QString callID)
{
  state_.endCall(callID);
}

void SIPManager::endAllCalls()
{
  state_.endAllCalls();
}
