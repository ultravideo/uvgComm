#include "sipclienttransaction.h"

#include "initiation/siptransactionuser.h"

#include "common.h"

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
  outMessage->content.type = NO_CONTENT;
  outMessage->content.length = 0;

  ViaInfo via = ViaInfo{TRANSPORTTYPE, "2.0", "", 0,
          QString("z9hG4bK" + generateRandomString(BRANCHLENGTH))};
  outMessage->vias.push_back(via);
}


void SIPClientTransaction::startTimer(RequestType type)
{
  // INVITE has the same timeout as rest of them. Only after RINGING reply do we increase timeout
  if(type != SIP_CANCEL && type != SIP_ACK)
  {
    startTimeoutTimer();
  }
}


void SIPClientTransaction::startTransaction(RequestType type)
{
  printDebug(DEBUG_NORMAL, this, DC_SEND_SIP_REQUEST,
             "Client starts sending a request.", {"Type"}, {QString::number(type)});
  ongoingTransactionType_ = type;

  // we do not expect a response for these requests.
  if (type == SIP_CANCEL || type == SIP_ACK)
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
  printDebug(DEBUG_NORMAL, this, DC_SEND_SIP, "No response. Request timed out.");
  processTimeout();
}
