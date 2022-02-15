#include "sipclient.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"
#include "logger.h"
#include "global.h"

#include <QVariant>

SIPClient::SIPClient():
  ongoingTransactionType_(SIP_NO_REQUEST),
  requestTimer_(),
  expires_(0),
  shouldLive_(true),
  activeRegistration_(false)
{
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}


SIPClient::~SIPClient()
{}


void SIPClient::processIncomingResponse(SIPResponse& response, QVariant& content,
                                        bool retryRequest)
{
  Logger::getLogger()->printNormal(this, "Client starts processing response");
  int responseCode = response.type;

  if (!checkTransactionType(response.message->cSeq.method))
  {
    Logger::getLogger()->printPeerError(this, "Their response transaction type "
                                              "is not the same as our request!");
    return;
  }

  emit incomingResponse(response, content, retryRequest);

  // Provisional response, continuing.
  // Refreshes timeout timer.
  if (responseCode >= 100 && responseCode <= 199)
  {
    Logger::getLogger()->printNormal(this, "Got a provisional response. Restarting timer.");
    if (response.message->cSeq.method == SIP_INVITE &&
        responseCode == SIP_RINGING && expires_ != 0)
    {
      // This is how long the call is allowed to ring at the callee end
      startTimeoutTimer(expires_);
    }
    else
    {
      startTimeoutTimer();
    }
  }
  else if (responseCode >= 200 && responseCode <= 299)
  {
    if (response.type == SIP_OK)
    {
      if (response.message->cSeq.method == SIP_INVITE)
      {
        sendRequest(SIP_ACK);
      }
      else if (ongoingTransactionType_ == SIP_BYE)
      {
        shouldLive_ = false;
      }
      else if (response.message->cSeq.method == SIP_REGISTER)
      {
        activeRegistration_ = !response.message->contact.empty();
      }
    }
  }
  else if (response.type >= 300 && response.type <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    Logger::getLogger()->printWarning(this, "Got a Redirection Response.");
  }
  else if (response.type >= 400 && response.type <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    Logger::getLogger()->printWarning(this, "Got a Failure Response.");
  }
  else if (response.type >= 500 && response.type <= 599)
  {
    Logger::getLogger()->printWarning(this, "Got a Server Failure Response.");
  }
  else if (response.type >= 600 && response.type <= 699)
  {
    Logger::getLogger()->printWarning(this, "Got a Global Failure Response.");
  }

  if (response.type >= 300 && response.type <= 699)
  {
    if (!retryRequest)
    {
      shouldLive_ = false;
    }
  }

  SIPRequestMethod previousTransactionMethod = ongoingTransactionType_;
  uint32_t previousExpires = expires_;

  // the transaction ends
  if (responseCode >= 200)
  {
    ongoingTransactionType_ = SIP_NO_REQUEST;
    stopTimeoutTimer();
    expires_ = 0;
  }

  if (retryRequest)
  {
    if (previousTransactionMethod == SIP_REGISTER ||
        previousTransactionMethod == SIP_INVITE)
    {
      sendRequest(previousTransactionMethod, previousExpires);
    }
    else
    {
      sendRequest(previousTransactionMethod);
    }

    return;
  }
}


SIPRequest SIPClient::generateRequest(SIPRequestMethod method)
{
  Q_ASSERT(method != SIP_NO_REQUEST);
  Q_ASSERT(method != SIP_UNKNOWN_REQUEST);

  SIPRequest request;
  request.method = method;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);

  Logger::getLogger()->printNormal(this, "Generating request");

  // we do not expect a response for these requests.
  if (request.method == SIP_CANCEL || request.method == SIP_ACK)
  {
    stopTimeoutTimer();
    // cancel and ack don't start a transaction
    if (request.method == SIP_ACK)
    {
      ongoingTransactionType_ = SIP_NO_REQUEST;
    }
  }
  else
  {
    ongoingTransactionType_ = request.method;
  }

  request.sipVersion = SIP_VERSION;

  // sets many of the mandatory fields in SIP header
  request.message->cSeq.cSeq = 0; // 0 is invalid, the correct value is set later
  request.message->cSeq.method = request.method;

  request.message->maxForwards
      = std::shared_ptr<uint8_t> (new uint8_t{DEFAULT_MAX_FORWARDS});

  request.message->contentType = MT_NONE;
  request.message->contentLength = 0;

  // via is set later by other components

  // INVITE has the same timeout as other requests. Only after RINGING reply do we
  // increase the timeout. CANCEL and ACK do not expect reply
  if(request.method != SIP_CANCEL && request.method != SIP_ACK)
  {
    startTimeoutTimer();
  }

  return request;
}


void SIPClient::processTimeout()
{
  Logger::getLogger()->printWarning(this, "Request timed out!");

  if(ongoingTransactionType_ == SIP_INVITE)
  {
    // send CANCEL request
    sendRequest(SIP_CANCEL);
  }
  else
  {
    requestTimer_.stop();
    ongoingTransactionType_ = SIP_NO_REQUEST;
  }
  if(ongoingTransactionType_ == SIP_BYE)
  {
    shouldLive_ = false;
    // TODO: Figure out a way to trigger deletion in case BYE timeout.
    // Probably through some sort of garbage collection.
  }
}


void SIPClient::requestTimeOut()
{
  Logger::getLogger()->printWarning(this, "No response. Request timed out.",
                                          {"Ongoing transaction"}, 
                                          {QString::number(ongoingTransactionType_)});
  processTimeout();
}


void SIPClient::refreshRegistration()
{
  if (!activeRegistration_)
  {
    Logger::getLogger()->printWarning(this, "We have no registrations when refreshing!");
  }

  sendREGISTER(REGISTER_INTERVAL);
}


void SIPClient::sendREGISTER(uint32_t timeout)
{
  Logger::getLogger()->printNormal(this, "Sending REGISTER request",
                                   {"Expires"}, QString::number(timeout));
  sendRequest(SIP_REGISTER, timeout);
}

void SIPClient::sendINVITE(uint32_t timeout)
{
  Logger::getLogger()->printNormal(this, "Sending INVITE request",
                                   {"Expires"}, QString::number(timeout));

  sendRequest(SIP_INVITE, timeout);
}


void SIPClient::sendBYE()
{
  Logger::getLogger()->printNormal(this, "Sending BYE request");

  sendRequest(SIP_BYE);
}


void SIPClient::sendCANCEL()
{
  Logger::getLogger()->printNormal(this, "Sending CANCEL request");

  sendRequest(SIP_CANCEL);
}


bool SIPClient::correctRequestType(SIPRequestMethod method)
{
  if (ongoingTransactionType_ != SIP_NO_REQUEST &&
      method != SIP_CANCEL && method != SIP_ACK)
  {
    Logger::getLogger()->printProgramWarning(this, "Tried to send a request "
                              "while previous transaction has not finished");
    return false;
  }
  else if (ongoingTransactionType_ == SIP_NO_REQUEST && method == SIP_CANCEL)
  {
    Logger::getLogger()->printProgramWarning(this, "Tried to cancel a "
                                                   "transaction that does not exist!");
    return false;
  }

  return true;
}


bool SIPClient::sendRequest(SIPRequestMethod method)
{
  if (!correctRequestType(method))
  {
    return false;
  }

  SIPRequest request = generateRequest(method);
  QVariant content;
  emit outgoingRequest(request, content);

  return true;
}


bool SIPClient::sendRequest(SIPRequestMethod method, uint32_t expires)
{
  if (!correctRequestType(method))
  {
    return false;
  }

  SIPRequest request = generateRequest(method);
  request.message->expires = std::shared_ptr<uint32_t> (new uint32_t{expires});
  expires_ = expires;
  QVariant content;
  emit outgoingRequest(request, content);

  return true;
}
