#include "sipsession.h"

#include <QDebug>

// 2 seconds for a SIP reply
const unsigned int TIMEOUT = 2000;

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPSession::SIPSession():
  session_(),
  sessionID_(0),
  ongoingTransactionType_(NOREQUEST),
  state_(CALL_INACTIVE),
  registered_(false),
  timeoutTimer_()
{}

void SIPSession::setStateInfo(SIPSessionInfo session, uint32_t sessionID)
{
  Q_ASSERT(sessionID_ != 0);
  session_ = session;
  sessionID_ = sessionID;

  timeoutTimer_.setSingleShot(true);
}

// checks that the incoming message belongs to this session
bool SIPSession::correctSession(const SIPSessionInfo& session) const
{
  return session.callID == session_.callID &&
      session.localTag == session_.localTag &&
      session.remoteTag == session_.remoteTag;
}

// processes incoming request
void SIPSession::processRequest(RequestType request, const SIPSessionInfo& session,
                                const SIPMessage& messageInfo)
{

}

//processes incoming response
void SIPSession::processResponse(ResponseType response, const SIPSessionInfo &session,
                                 const SIPMessage& messageInfo)
{

}

void SIPSession::startCall()
{
  if(state_ == CALL_INACTIVE)
  {
    emit sendRequest(sessionID_, INVITE, session_);
  }
  else
  {
    qDebug() << "WARNING: Trying to start a call, when it is already running or negotiating!";
  }

  requestSender(INVITE);
}

void SIPSession::endCall()
{
  if(state_ != CALL_ACTIVE)
  {
    qDebug() << "WARNING: Trying to end a non-active call!";
    return;
  }

  requestSender(BYE);
}

void SIPSession::registerToServer()
{
  if(registered_ || ongoingTransactionType_ != NOREQUEST)
  {
    qDebug() << "WARNING: We have already registered";
  }

  requestSender(REGISTER);
}

void SIPSession::sendMessage(QString message)
{
  qDebug() << "WARNING: Sending messages has not been implemented!";
}

void SIPSession::requestSender(RequestType type)
{
  emit sendRequest(sessionID_, type, session_);
  ongoingTransactionType_ = type;

  if(type != INVITE)
  {
    timeoutTimer_.start(TIMEOUT);
  }
  else
  {
    timeoutTimer_.start(INVITE_TIMEOUT);
  }
}

void SIPSession::requestTimeOut()
{
  qDebug() << "No response. Request timed out";

  if(ongoingTransactionType_ == INVITE)
  {
    sendRequest(sessionID_, BYE, session_);
  }

  emit timedOut(sessionID_, ongoingTransactionType_);
  ongoingTransactionType_ = NOREQUEST;
}
