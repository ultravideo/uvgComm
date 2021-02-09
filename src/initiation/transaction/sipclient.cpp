#include "sipclient.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

#include <QVariant>

SIPClient::SIPClient():
  ongoingTransactionType_(SIP_NO_REQUEST),
  requestTimer_(),
  expires_(nullptr)
{
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}


SIPClient::~SIPClient()
{}


void SIPClient::setNextTransactionExpires(uint32_t timeout)
{
  expires_ = std::shared_ptr<uint32_t> (new uint32_t{timeout});
}


void SIPClient::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  printNormal(this, "Client starts processing response");

  int responseCode = response.type;

  if (!checkTransactionType(response.message->cSeq.method))
  {
    printPeerError(this, "Their response transaction type is not the same as our request!");

    emit failure("Received wrong transaction type");
    return;
  }

  // Provisional response, continuing.
  // Refreshes timeout timer.
  if (responseCode >= 100 && responseCode <= 199)
  {
    printNormal(this, "Got a provisional response. Restarting timer.");
    if (response.message->cSeq.method == SIP_INVITE &&
        responseCode == SIP_RINGING && expires_ != nullptr)
    {
      startTimeoutTimer(*expires_);
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
    expires_ = nullptr;
  }

  emit incomingResponse(response, content);
}


void SIPClient::generateRequest(SIPRequestMethod type,
                                SIPRequest& request)
{
  request.sipVersion = SIP_VERSION;
  request.method = type;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);

  // sets many of the mandatory fields in SIP header
  request.message->cSeq.cSeq = 0; // invalid, should be set in dialog
  request.message->cSeq.method = type;

  request.message->maxForwards
      = std::shared_ptr<uint8_t> (new uint8_t{DEFAULT_MAX_FORWARDS});

  request.message->contentType = MT_NONE;
  request.message->contentLength = 0;

  // via is set later

  if (expires_ != nullptr &&
      (request.method == SIP_REGISTER || request.method == SIP_INVITE))
  {
    request.message->expires = expires_;
  }

  // INVITE has the same timeout as rest of them. Only after RINGING reply do we
  // increase the timeout. CANCEL and ACK have no reply
  if(request.method != SIP_CANCEL && request.method != SIP_ACK)
  {
    startTimeoutTimer();
  }
}


bool SIPClient::sendRequest(SIPRequestMethod type)
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
    // cancel and ack don't start a transaction
    ongoingTransactionType_ = SIP_NO_REQUEST;
  }
  else
  {
    ongoingTransactionType_ = type;
  }
  SIPRequest request;
  generateRequest(type, request);

  QVariant content;
  emit outgoingRequest(request, content);

  return true;
}


void SIPClient::processTimeout()
{
  emit failure("No response. Request timed out");

  if(ongoingTransactionType_ == SIP_INVITE)
  {
    sendRequest(SIP_CANCEL);
  }

  requestTimer_.stop();

  ongoingTransactionType_ = SIP_NO_REQUEST;

  // TODO: Figure out a way to delete in case BYE timeouts
}


void SIPClient::requestTimeOut()
{
  printWarning(this, "No response. Request timed out.",
    {"Ongoing transaction"}, {QString::number(ongoingTransactionType_)});
  processTimeout();
}

