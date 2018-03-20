#include "sipclienttransaction.h"

#include "siptransactionuser.h"

#include <QDebug>

// 2 seconds for a SIP reply
const unsigned int TIMEOUT = 2000;
// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPClientTransaction::SIPClientTransaction():
  ongoingTransactionType_(SIP_UNKNOWN_REQUEST),
  connected_(false),
  sessionID_(0),
  state_(INACTIVE),
  pendingRequest_(SIP_UNKNOWN_REQUEST),
  transactionUser_(NULL)
{}

void SIPClientTransaction::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  Q_ASSERT(tu);
  transactionUser_ = tu;
  sessionID_ = sessionID;
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}


//processes incoming response
void SIPClientTransaction::processResponse(SIPResponse &response)
{
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    qWarning() << "WARNING: SIP Client Transaction not initialized.";
    return;
  }

  qWarning() << "WARNING: Response processing in session not implemented.";

  if(ongoingTransactionType_ != SIP_UNKNOWN_REQUEST)
  {
    ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
  }
}


bool SIPClientTransaction::startCall()
{
  qDebug() << "Starting a call and sending an INVITE in session";
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    qWarning() << "WARNING: SIP Client Transaction not initialized";
    return false;
  }

  if(state_ == INACTIVE)
  {
    requestSender(INVITE);
  }
  else
  {
    qWarning() << "WARNING: Trying to start a call, when it is already running or negotiating:" << state_;
    return false;
  }

  return true;
}

void SIPClientTransaction::endCall()
{
  if(state_ != RUNNNING)
  {
    qDebug() << "WARNING: Trying to end a non-active call!";
    return;
  }

  requestSender(BYE);
}

void SIPClientTransaction::registerToServer()
{
  requestSender(REGISTER);
}

void SIPClientTransaction::connectionReady(bool ready)
{
  connected_ = ready;
  if(pendingRequest_ != SIP_UNKNOWN_REQUEST)
  {
    requestSender(pendingRequest_);
  }
}

void SIPClientTransaction::requestSender(RequestType type)
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
      requestTimer_.start(TIMEOUT);
    }
    else
    {
      qDebug() << "INVITE timeout set to: " << INVITE_TIMEOUT;
      requestTimer_.start(INVITE_TIMEOUT);
    }
  }
}

void SIPClientTransaction::requestTimeOut()
{
  qDebug() << "No response. Request timed out";

  if(ongoingTransactionType_ == INVITE)
  {
    sendRequest(sessionID_, BYE);
    // TODO tell user we have failed
  }

  requestSender(CANCEL);
  transactionUser_->callRejected(sessionID_);
  ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
}
