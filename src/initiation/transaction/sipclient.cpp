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


void SIPClient::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  printNormal(this, "Processing outgoing request");

  if (ongoingTransactionType_ != SIP_NO_REQUEST && request.method != SIP_CANCEL)
  {
    printProgramWarning(this, "Tried to send a request "
                              "while previous transaction has not finished");
    return;
  }
  else if (ongoingTransactionType_ == SIP_NO_REQUEST && request.method == SIP_CANCEL)
  {
    printProgramWarning(this, "Tried to cancel a transaction that does not exist!");
    return;
  }

  // we do not expect a response for these requests.
  if (request.method == SIP_CANCEL || request.method == SIP_ACK)
  {
    stopTimeoutTimer();
    // cancel and ack don't start a transaction
    ongoingTransactionType_ = SIP_NO_REQUEST;
  }
  else
  {
    ongoingTransactionType_ = request.method;
  }

  generateRequest(request);
  emit outgoingRequest(request, content);
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


void SIPClient::generateRequest(SIPRequest& request)
{
  Q_ASSERT(request.method != SIP_NO_REQUEST);
  Q_ASSERT(request.method != SIP_UNKNOWN_REQUEST);
  Q_ASSERT(request.message != nullptr);

  request.sipVersion = SIP_VERSION;

  // sets many of the mandatory fields in SIP header
  request.message->cSeq.cSeq = 0; // invalid, should be set in dialog
  request.message->cSeq.method = request.method;

  request.message->maxForwards
      = std::shared_ptr<uint8_t> (new uint8_t{DEFAULT_MAX_FORWARDS});

  request.message->contentType = MT_NONE;
  request.message->contentLength = 0;

  // via is set later

  if (request.message->expires != nullptr)
  {
     expires_ = request.message->expires;
  }

  // INVITE has the same timeout as rest of them. Only after RINGING reply do we
  // increase the timeout. CANCEL and ACK have no reply
  if(request.method != SIP_CANCEL && request.method != SIP_ACK)
  {
    startTimeoutTimer();
  }
}

void SIPClient::processTimeout()
{
  emit failure("No response. Request timed out");

  if(ongoingTransactionType_ == SIP_INVITE)
  {
    // send CANCEL request

    SIPRequest request;
    request.method = SIP_CANCEL;
    request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
    QVariant content;

    processOutgoingRequest(request, content);
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
