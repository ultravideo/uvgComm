#include "sipsinglecall.h"

#include "common.h"
#include "logger.h"

#include <QVariant>


// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;


SIPSingleCall::SIPSingleCall():
  transactionUser_(nullptr),
  sessionID_(0),
  shouldLive_(true),
  triedAuthenticating_(false)
{}


void SIPSingleCall::init(SIPTransactionUser* tu,
                         uint32_t sessionID)
{
  transactionUser_ = tu;
  sessionID_ = sessionID;
}


void SIPSingleCall::startCall(QString callee)
{
  Logger::getLogger()->printNormal(this, "Starting a call and sending an INVITE in session");
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "SIP Client Transaction not initialized.");
    return;
  }

  SIPRequest request = createRequest(SIP_INVITE);
  setExpires(INVITE_TIMEOUT, request.message);
  QVariant content;
  emit outgoingRequest(request, content);

  transactionUser_->outgoingCall(sessionID_, callee);
}


void SIPSingleCall::endCall()
{
  SIPRequest request = createRequest(SIP_BYE);
  QVariant content;
  emit outgoingRequest(request, content);
}


void SIPSingleCall::cancelOutgoingCall()
{
  SIPRequest request = createRequest(SIP_CANCEL);
  QVariant content;
  emit outgoingRequest(request, content);
}


void SIPSingleCall::acceptIncomingCall()
{
  SIPResponse response = createResponse(SIP_OK);
  QVariant content;
  emit outgoingResponse(response, content);
}


void SIPSingleCall::declineIncomingCall()
{
  SIPResponse response = createResponse(SIP_DECLINE);
  QVariant content;
  emit outgoingResponse(response, content);
}


void SIPSingleCall::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Processing incoming request");

  Q_UNUSED(content);
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
               "SIP Server transaction not initialized.");
    shouldLive_ = false;
    return;
  }

  switch(request.method)
  {
  case SIP_INVITE:
  {
    if (!transactionUser_->incomingCall(sessionID_,
                                        request.message->from.address.realname))
    {
      // if we did not auto-accept
      SIPResponse response = createResponse(SIP_RINGING);
      QVariant content;
      emit outgoingResponse(response, content);
    }
    else
    {
      SIPResponse response = createResponse(SIP_OK);
      QVariant content;
      emit outgoingResponse(response, content);
    }
    break;
  }
  case SIP_ACK:
  {
    transactionUser_->callNegotiated(sessionID_);
    break;
  }
  case SIP_BYE:
  {
    SIPResponse response = createResponse(SIP_OK);
    QVariant content;
    emit outgoingResponse(response, content);

    // this takes too long, send response first.
    transactionUser_->endCall(sessionID_);
    shouldLive_ = false;
    return;
  }
  case SIP_CANCEL:
  {
    transactionUser_->cancelIncomingCall(sessionID_);
    SIPResponse response = createResponse(SIP_REQUEST_TERMINATED);
    QVariant content;
    emit outgoingResponse(response, content);
    shouldLive_ = false;
    return;
  }
  case SIP_OPTIONS:
  {
    Logger::getLogger()->printUnimplemented(this, "OPTIONS-request not implemented yet");
    break;
  }
  case SIP_REGISTER:
  {
    Logger::getLogger()->printPeerError(this, "REGISTER-method detected. We are not a registrar!!");
    SIPResponse response = createResponse(SIP_NOT_ALLOWED);
    QVariant content;
    emit outgoingResponse(response, content);
    break;
  }
  default:
  {
    Logger::getLogger()->printUnimplemented(this, "Unsupported request type received");
    SIPResponse response = createResponse(SIP_NOT_ALLOWED);
    QVariant content;
    emit outgoingResponse(response, content);
    break;
  }
  }
  shouldLive_ = true;
  return;

}


void SIPSingleCall::processIncomingResponse(SIPResponse& response, QVariant& content)
{ 
  Q_UNUSED(content);

  Logger::getLogger()->printNormal(this, "Processing incoming response");

  if (response.type >= 100 && response.type <= 299)
  {
    // process anything that is not a failure and may cause a new request to be sent.
    // this must be done after the SIPClientTransaction processResponse, because that checks our
    // current active transaction and may modify it.
    if(response.message->cSeq.method == SIP_INVITE)
    {
      if(response.type == SIP_RINGING)
      {
        transactionUser_->callRinging(sessionID_);
      }
      else if(response.type == SIP_OK)
      {
        transactionUser_->peerAccepted(sessionID_);

        SIPRequest request = createRequest(SIP_ACK);
        QVariant content;
        emit outgoingRequest(request, content);

        transactionUser_->callNegotiated(sessionID_);
      }
    }
    else if (response.message->cSeq.method == SIP_BYE)
    {
      transactionUser_->endCall(sessionID_);
    }
    shouldLive_ = true;
  }
  else if (response.type >= 300 && response.type <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    Logger::getLogger()->printWarning(this, "Got a Redirection Response.");
    shouldLive_ = false;
  }
  else if (response.type >= 400 && response.type <= 499)
  {
    if (response.type == SIP_PROXY_AUTHENTICATION_REQUIRED && !triedAuthenticating_)
    {
      triedAuthenticating_ = true;

      SIPRequest request = createRequest(response.message->cSeq.method);
      QVariant content;

      if (response.message->cSeq.method == SIP_INVITE)
      {
        setExpires(INVITE_TIMEOUT, request.message);
      }

      emit outgoingRequest(request, content);
      return;
    }

    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    Logger::getLogger()->printWarning(this, "Got a Failure Response.");
    shouldLive_ = false;
  }
  else if (response.type >= 500 && response.type <= 599)
  {
    Logger::getLogger()->printWarning(this, "Got a Server Failure Response.");
    shouldLive_ = false;
  }
  else if (response.type >= 600 && response.type <= 699)
  {
    Logger::getLogger()->printWarning(this, "Got a Global Failure Response.");
    if (response.message->cSeq.method == SIP_INVITE)
    {
      if (response.type == SIP_DECLINE)
      {
        transactionUser_->endCall(sessionID_);
      }
    }
    shouldLive_ = false;
  }

  triedAuthenticating_ = false;
}


void SIPSingleCall::processFailure(QString message)
{
  transactionUser_->failure(sessionID_, message);
  shouldLive_ = false;
}


void SIPSingleCall::setExpires(uint32_t timeout,
                               std::shared_ptr<SIPMessageHeader> header)
{
  header->expires = std::shared_ptr<uint32_t> (new uint32_t{timeout});
}


SIPRequest SIPSingleCall::createRequest(SIPRequestMethod method)
{
  SIPRequest request;
  request.method = method;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);

  return request;
}


SIPResponse SIPSingleCall::createResponse(SIPResponseStatus status)
{
  SIPResponse response;
  response.type = status;
  response.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
  return response;
}

