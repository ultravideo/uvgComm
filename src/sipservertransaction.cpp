#include "sipservertransaction.h"

#include "siptransactionuser.h"

#include <QDebug>

SIPServerTransaction::SIPServerTransaction():
  sessionID_(0),
  transactionUser_(NULL)
{

}

void SIPServerTransaction::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  transactionUser_ = tu;
}

// processes incoming request
void SIPServerTransaction::processRequest(SIPRequest &request)
{
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    qWarning() << "WARNING: SIP Session not initialized.";
    return;
  }

  // TODO: check that the request is appropriate at this time.

  switch(request.type)
  {
  case INVITE:
  {
    transactionUser_->incomingCall(sessionID_, request.message->routing->to.realname);
    responseSender(SIP_RINGING, request.type);
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
    responseSender(SIP_NOT_IMPLEMENTED, request.type);
    break;
  }
  default:
  {
    qDebug() << "Unsupported request type received";
    responseSender(SIP_NOT_IMPLEMENTED, request.type);
    break;
  }
  }
}

void SIPServerTransaction::responseSender(ResponseType type, RequestType originalRequest)
{
   emit sendResponse(sessionID_, type, originalRequest);
}

