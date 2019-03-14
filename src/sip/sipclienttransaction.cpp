#include "sipclienttransaction.h"

#include "siptransactionuser.h"

#include <QDebug>



SIPClientTransaction::SIPClientTransaction(SIPTransactionUser* tu):
  ongoingTransactionType_(SIP_NO_REQUEST),
  transactionUser_(tu)
{}

void SIPClientTransaction::init()
{
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}

void SIPClientTransaction::getRequestMessageInfo(RequestType type,
                                                 std::shared_ptr<SIPMessageInfo>& outMessage)
{
  outMessage = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo);

  if(type == SIP_ACK)
  {
    outMessage->transactionRequest = SIP_INVITE;
  }
  else if(type == SIP_CANCEL)
  {
    outMessage->transactionRequest = ongoingTransactionType_;
  }
  else
  {
    outMessage->transactionRequest = type;
  }

  outMessage->dialog = nullptr;
  outMessage->maxForwards = 71;
  outMessage->version = "2.0";
  outMessage->cSeq = 0; // invalid, should be set in dialog
}


void SIPClientTransaction::sendRequest(RequestType type)
{
  qDebug() << "Client starts sending a request:" << type;
  ongoingTransactionType_ = type;


  // INVITE has the same timeout as rest of them. Only after RINGING reply do we increase timeout
  if(type != SIP_CANCEL && type != SIP_ACK)
  {
    startTimeoutTimer();
  }
  else
  {
    stopTimeoutTimer();
  }
}


void SIPClientTransaction::processTimeout()
{
  requestTimer_.stop();
  ongoingTransactionType_ = SIP_NO_REQUEST;
}


void SIPClientTransaction::requestTimeOut()
{
  qDebug() << "No response. Request timed out";
  processTimeout();
}
