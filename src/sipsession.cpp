#include "sipsession.h"
#include "common.h"

#include "callcontrolinterface.h"

#include <QDebug>

// 2 seconds for a SIP reply
const unsigned int TIMEOUT = 2000;
// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;

SIPSession::SIPSession():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  sessionID_(0),
  cSeq_(1),
  ongoingTransactionType_(SIP_UNKNOWN_REQUEST),
  state_(CALL_INACTIVE),
  registered_(false),
  timeoutTimer_(),
  connected_(false),
  pendingRequest_(SIP_UNKNOWN_REQUEST),
  pendingResponse_(SIP_UNKNOWN_RESPONSE)
{}

void SIPSession::init(uint32_t sessionID, CallControlInterface *callControl)
{
  Q_ASSERT(sessionID != 0);
  Q_ASSERT(callControl);
  callControl_ = callControl;
  localTag_ = generateRandomString(TAGLENGTH);
  sessionID_ = sessionID;
  timeoutTimer_.setSingleShot(true);
  connect(&timeoutTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}

std::shared_ptr<SIPSessionInfo> SIPSession::getRequestInfo()
{
  return std::shared_ptr<SIPSessionInfo> (new SIPSessionInfo{remoteTag_, localTag_, callID_});
}

std::shared_ptr<SIPSessionInfo> SIPSession::getResponseInfo()
{
  Q_ASSERT(ongoingTransactionType_ != SIP_UNKNOWN_REQUEST);
  return std::shared_ptr<SIPSessionInfo> (new SIPSessionInfo{localTag_, remoteTag_, callID_});
}

bool SIPSession::correctRequest(std::shared_ptr<SIPSessionInfo> session)
{
  return (session->toTag == localTag_ || session->toTag == "") && (session->fromTag == remoteTag_ || remoteTag_ == "") &&
      ( session->callID == callID_ || callID_ == "");
}

bool SIPSession::correctResponse(std::shared_ptr<SIPSessionInfo> session)
{
  return session->fromTag == localTag_ && (session->toTag == remoteTag_ || remoteTag_ == "") &&
      ( session->callID == callID_ || callID_ == "");
}

std::shared_ptr<SIPMessageInfo> SIPSession::generateMessage(RequestType originalRequest)
{
  std::shared_ptr<SIPMessageInfo> mesg = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  mesg->session = NULL;
  mesg->routing = NULL;
  mesg->cSeq = cSeq_;
  ++cSeq_;
  if(originalRequest == ACK)
  {
    mesg->transactionRequest = INVITE;
  }
  else
  {
    mesg->transactionRequest = originalRequest;
  }
  return mesg;
}

// processes incoming request
void SIPSession::processRequest(SIPRequest &request)
{
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    qWarning() << "WARNING: SIP Session not initialized.";
    return;
  }

  // TODO: check that the request is appropriate at this time.

  switch(request.type)
  {
  case INVITE:
  {
    callControl_->incomingCall(sessionID_, request.message->routing->to.realname);
    responseSender(SIP_RINGING);
    break;
  }
  case ACK:
  {
    callControl_->callNegotiated(sessionID_);
    break;
  }
  case BYE:
  {
    callControl_->endCall(sessionID_);
    break;
  }
  case CANCEL:
  {
    emit callControl_->cancelIncomingCall(sessionID_);
    break;
  }
  case OPTIONS:
  {
    qDebug() << "Don't know what to do with OPTIONS yet";
    break;
  }
  case REGISTER:
  {
    qDebug() << "Why on earth are we receiving REGISTER methods?";
    responseSender(SIP_NOT_IMPLEMENTED);
    break;
  }
  default:
  {
    qDebug() << "Unsupported request type received";
    responseSender(SIP_NOT_IMPLEMENTED);
    break;
  }
  }
}

//processes incoming response
void SIPSession::processResponse(SIPResponse &response)
{
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    qWarning() << "WARNING: SIP Session not initialized.";
    return;
  }
}

bool SIPSession::startCall()
{
  qDebug() << "Starting a call and sending an INVITE in session";
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    qWarning() << "WARNING: SIP Session not initialized";
    return false;
  }

  if(state_ == CALL_INACTIVE)
  {
    if(callID_ == "")
    {
      callID_ = generateRandomString(CALLIDLENGTH);
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

void SIPSession::connectionReady(bool ready)
{
  connected_ = ready;
  if(pendingRequest_ != SIP_UNKNOWN_REQUEST)
  {
    requestSender(pendingRequest_);
  }
  if(pendingResponse_ != SIP_UNKNOWN_RESPONSE)
  {
    responseSender(pendingResponse_);
  }
}

void SIPSession::requestSender(RequestType type)
{
  if(!connected_)
  {
    pendingRequest_ = type;
  }
  else
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
}

void SIPSession::responseSender(ResponseType type)
{
  if(!connected_)
  {
    pendingResponse_ = type;
  }
  else
  {
    emit sendResponse(sessionID_, type);
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

  requestSender(CANCEL);
  callControl_->callRejected(sessionID_);
  ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
}
