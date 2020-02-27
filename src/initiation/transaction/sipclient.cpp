#include "sipclient.h"

#include "initiation/siptransactionuser.h"

#include "common.h"

#include <QDebug>

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPClient::SIPClient():
  ongoingTransactionType_(SIP_NO_REQUEST)
{
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}


bool SIPClient::processResponse(SIPResponse& response,
                                SIPDialogState &state)
{
  Q_UNUSED(state);

  int responseCode = response.type;

  if (!checkTransactionType(response.message->transactionRequest))
  {
    printPeerError(this, "Their response transaction type is not the same as our request!");
    return false;
  }

  // provisional response, continuing
  if (responseCode >= 100 && responseCode <= 199)
  {
    printNormal(this, "Got a provisional response. Restarting timer.");
    if (response.message->transactionRequest == SIP_INVITE &&
        responseCode == SIP_RINGING)
    {
      startTimeoutTimer(INVITE_TIMEOUT);
    }
    else
    {
      startTimeoutTimer();
    }
  }

  // the transaction ends
  if (responseCode >= 200)
  {
    ongoingTransactionType_ = SIP_NO_REQUEST;
    stopTimeoutTimer();
  }

  if (responseCode >= 300 && responseCode <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    printWarning(this, "Got a Redirection Response.");
  }
  else if (responseCode >= 400 && responseCode <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    printWarning(this, "Got a Failure Response.");
  }
  else if (responseCode >= 500 && responseCode <= 599)
  {
    printWarning(this, "Got a Server Failure Response.");
  }
  else if (responseCode >= 600 && responseCode <= 699)
  {
    printWarning(this, "Got a Global Failure Response.");
  }

  // delete dialog if response was not provisional or success.
  // TODO: this is a very simple logic. More complex logic could handle different
  // situations
  return responseCode >= 100 && responseCode <= 299;
}


void SIPClient::getRequestMessageInfo(RequestType type,
                                                 std::shared_ptr<SIPMessageInfo>& outMessage)
{
  outMessage = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo);

  if(type == SIP_CANCEL)
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
          QString("z9hG4bK" + generateRandomString(BRANCHLENGTH)), false, false, 0, ""};
  outMessage->vias.push_back(via);

  // INVITE has the same timeout as rest of them. Only after RINGING reply do we increase timeout
  if(type != SIP_CANCEL && type != SIP_ACK)
  {
    startTimeoutTimer();
  }
}


bool SIPClient::startTransaction(RequestType type)
{
  printDebug(DEBUG_NORMAL, this,
             "Client starts sending a request.", {"Type"}, {QString::number(type)});

  if (ongoingTransactionType_ != SIP_NO_REQUEST && type != SIP_CANCEL)
  {
    printProgramWarning(this, "Tried to send a request "
                              "while previous transaction has not finished");
    return false;
  }
  else if (ongoingTransactionType_ == SIP_NO_REQUEST && type == SIP_CANCEL)
  {
    printProgramWarning(this, "Tried to cancel a transaction that does not exist!");
    return false;
  }

  // we do not expect a response for these requests.
  if (type == SIP_CANCEL || type == SIP_ACK)
  {
    stopTimeoutTimer();
  }
  else
  {
    // cancel and ack don't start a transaction
    ongoingTransactionType_ = type;
  }

  return true;
}


void SIPClient::processTimeout()
{
  requestTimer_.stop();
  ongoingTransactionType_ = SIP_NO_REQUEST;
}


void SIPClient::requestTimeOut()
{
  printWarning(this, "No response. Request timed out.",
    {"Ongoing transaction"}, {QString::number(ongoingTransactionType_)});
  processTimeout();
}
