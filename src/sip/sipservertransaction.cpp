#include "sipservertransaction.h"

#include "siptransactionuser.h"

#include <QDebug>

SIPServerTransaction::SIPServerTransaction():
  sessionID_(0),
  waitingResponse_(SIP_UNKNOWN_REQUEST),
  transactionUser_(NULL)
{}

void SIPServerTransaction::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  transactionUser_ = tu;
  sessionID_ = sessionID;
}

// processes incoming request
void SIPServerTransaction::processRequest(SIPRequest &request)
{
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    qWarning() << "WARNING: SIP Server transaction not initialized.";
    return;
  }

  // TODO: check that the request is appropriate at this time.

  waitingResponse_ = request.type;

  switch(waitingResponse_)
  {
  case INVITE:
  {
    transactionUser_->incomingCall(sessionID_, request.message->to.realname);
    responseSender(SIP_RINGING, false);
    break;
  }
  case ACK:
  {
    transactionUser_->callNegotiated(sessionID_);
    break;
  }
  case BYE:
  {
    transactionUser_->endCall(sessionID_);
    break;
  }
  case CANCEL:
  {
    transactionUser_->cancelIncomingCall(sessionID_);
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
    responseSender(SIP_NOT_IMPLEMENTED, true);
    break;
  }
  default:
  {
    qDebug() << "Unsupported request type received";
    responseSender(SIP_NOT_IMPLEMENTED, true);
    break;
  }
  }
}

void SIPServerTransaction::acceptCall()
{
  responseSender(SIP_OK, true);
}

void SIPServerTransaction::rejectCall()
{
  responseSender(SIP_DECLINE, true);
}

void SIPServerTransaction::responseSender(ResponseType type, bool finalResponse)
{
  Q_ASSERT(waitingResponse_ != SIP_UNKNOWN_REQUEST);
  emit sendResponse(sessionID_, type, waitingResponse_);
  if(finalResponse)
  {
    waitingResponse_ = SIP_UNKNOWN_REQUEST;
  }
}
