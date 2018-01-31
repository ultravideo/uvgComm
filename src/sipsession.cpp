#include "sipsession.h"

#include <QDebug>

// 2 seconds for a SIP reply
const unsigned int TIMEOUT = 2000;
// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;


//TODO use cryptographically secure callID generation to avoid collisions.
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;
const uint16_t BRANCHLENGTH = 9;

SIPSession::SIPSession():
  session_(),
  sessionID_(0),
  cSeq_(1),
  ongoingTransactionType_(SIP_UNKNOWN_REQUEST),
  state_(CALL_INACTIVE),
  registered_(false),
  timeoutTimer_()
{}

void SIPSession::init(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  session_ = std::shared_ptr<SIPSessionInfo> (new SIPSessionInfo);
  session_->callID = "";
  session_->localTag = generateRandomString(TAGLENGTH);
  session_->remoteTag = "";

  sessionID_ = sessionID;
  timeoutTimer_.setSingleShot(true);
  connect(&timeoutTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}

void SIPSession::setSessionInfo(std::shared_ptr<SIPSessionInfo> info, uint32_t sessionID)
{
  Q_ASSERT(sessionID_ != 0);
  session_ = info;
  sessionID_ = sessionID;
}

std::shared_ptr<SIPSessionInfo> SIPSession::getSessionInfo()
{
  Q_ASSERT(session_);
  if(!session_)
  {
    qWarning() << "WARNING: No initialized session to give";
  }
  return session_;
}

SIPMessageInfo SIPSession::generateMessage(RequestType originalRequest)
{
  SIPMessageInfo mesg;
  mesg.branch = "z9hG4bK" + generateRandomString(BRANCHLENGTH);
  mesg.cSeq = cSeq_;
  ++cSeq_;
  if(originalRequest == ACK)
  {
    mesg.transactionRequest = INVITE;
  }
  else
  {
    mesg.transactionRequest = originalRequest;
  }
  return mesg;
}

// checks that the incoming message belongs to this session
bool SIPSession::correctSession(const SIPSessionInfo& session) const
{
  return session.callID == session_->callID &&
      session.localTag == session_->localTag &&
      session.remoteTag == session_->remoteTag;
}

// processes incoming request
void SIPSession::processRequest(RequestType request, const SIPSessionInfo& session,
                                const SIPMessageInfo& messageInfo)
{
  Q_ASSERT(sessionID_ != 0);
  if(!session_)
  {
    qWarning() << "WARNING: SIP Session not initialized.";
    return;
  }
}

//processes incoming response
void SIPSession::processResponse(ResponseType response, const SIPSessionInfo &session,
                                 const SIPMessageInfo& messageInfo)
{
  Q_ASSERT(sessionID_ != 0);
  if(!session_)
  {
    qWarning() << "WARNING: SIP Session not initialized.";
    return;
  }

}

bool SIPSession::startCall()
{
  qDebug() << "Starting a call and sending an INVITE in session";
  Q_ASSERT(sessionID_ != 0);
  if(!session_)
  {
    qWarning() << "WARNING: SIP Session not initialized";
    return false;
  }

  if(state_ == CALL_INACTIVE)
  {
    if(session_->callID == "")
    {
      session_->callID = generateRandomString(CALLIDLENGTH);
    }

    requestSender(INVITE);
  }
  else
  {
    qWarning() << "WARNING: Trying to start a call, when it is already running or negotiating:" << state_;
    return false;
  }

  return true;
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
  if(registered_ || ongoingTransactionType_ != SIP_UNKNOWN_REQUEST)
  {
    qDebug() << "WARNING: We have already registered";
  }

  requestSender(REGISTER);
}

void SIPSession::requestSender(RequestType type)
{
  emit sendRequest(sessionID_, type);
  ongoingTransactionType_ = type;

  if(type != INVITE)
  {
    qDebug() << "Request timeout set to: " << TIMEOUT;
    timeoutTimer_.start(TIMEOUT);
  }
  else
  {
    qDebug() << "INVITE timeout set to: " << INVITE_TIMEOUT;
    timeoutTimer_.start(INVITE_TIMEOUT);
  }
}

void SIPSession::requestTimeOut()
{
  qDebug() << "No response. Request timed out";

  if(ongoingTransactionType_ == INVITE)
  {
    sendRequest(sessionID_, BYE);
    // TODO tell user we have failed
  }

  // TODO emit some signal indicating something
  emit callFailed(sessionID_);
  ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
}

QString SIPSession::generateRandomString(uint32_t length)
{
  // TODO make this cryptographically secure to avoid collisions
  QString string;
  for( unsigned int i = 0; i < length; ++i )
  {
    string.append(alphabet.at(qrand()%alphabet.size()));
  }
  return string;
}
