#include "sipsinglecall.h"

#include "common.h"


// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;


SIPSingleCall::SIPSingleCall():
  client_(nullptr),
  server_(nullptr),
  transactionUser_(nullptr),
  sessionID_(0),
  shouldLive_(true)
{}


void SIPSingleCall::init(std::shared_ptr<SIPClient> client,
                         std::shared_ptr<SIPServer> server,
                         SIPTransactionUser* tu,
                         uint32_t sessionID)
{
  client_ = client;

  connect(client_.get(), &SIPClient::incomingResponse,
          this,         &SIPSingleCall::processIncomingResponse);

  connect(client_.get(), &SIPClient::outgoingRequest,
          this,         &SIPSingleCall::transmitRequest);

  connect(client_.get(), &SIPClient::failure,
          this,         &SIPSingleCall::processFailure);

  server_ = server;

  connect(server_.get(), &SIPServer::incomingRequest,
          this,          &SIPSingleCall::processIncomingRequest);

  connect(server_.get(), &SIPServer::outgoingResponse,
          this,          &SIPSingleCall::transmitResponse);


  transactionUser_ = tu;
  sessionID_ = sessionID;
}


void SIPSingleCall::startCall(QString callee)
{
  printNormal(this, "Starting a call and sending an INVITE in session");
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    printDebug(DEBUG_WARNING, this, "SIP Client Transaction not initialized.");
    return;
  }
  client_->setNextTransactionExpires(INVITE_TIMEOUT);
  client_->sendRequest(SIP_INVITE);

  transactionUser_->outgoingCall(sessionID_, callee);
}


void SIPSingleCall::endCall()
{
  Q_ASSERT(sessionID_);
  client_->sendRequest(SIP_BYE);
}


void SIPSingleCall::cancelOutgoingCall()
{
  Q_ASSERT(sessionID_);
  client_->sendRequest(SIP_CANCEL);
}


void SIPSingleCall::acceptIncomingCall()
{
  server_->respond(SIP_OK);
}


void SIPSingleCall::declineIncomingCall()
{
  server_->respond(SIP_DECLINE);
}


void SIPSingleCall::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
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
      server_->respond(SIP_RINGING);
    }
    else
    {
      server_->respond(SIP_OK);
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
    server_->respond(SIP_OK);

    // this takes too long, send response first.
    transactionUser_->endCall(sessionID_);
    shouldLive_ = false;
    return;
  }
  case SIP_CANCEL:
  {
    transactionUser_->cancelIncomingCall(sessionID_);
    // TODO: send 487 for INVITE
    shouldLive_ = false;
    return;
  }
  case SIP_OPTIONS:
  {
    printUnimplemented(this, "OPTIONS-request not implemented yet");
    break;
  }
  case SIP_REGISTER:
  {
    printPeerError(this, "REGISTER-method detected. We are not a registrar!!");
    server_->respond(SIP_NOT_ALLOWED);
    break;
  }
  default:
  {
    printUnimplemented(this, "Unsupported request type received");
    server_->respond(SIP_NOT_ALLOWED);
    break;
  }
  }
  shouldLive_ = true;
  return;

}


void SIPSingleCall::processIncomingResponse(SIPResponse& response, QVariant& content)
{ 
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

        client_->sendRequest(SIP_ACK);
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
    printWarning(this, "Got a Redirection Response.");
    shouldLive_ = false;
  }
  else if (response.type >= 400 && response.type <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    printWarning(this, "Got a Failure Response.");
    shouldLive_ = false;
  }
  else if (response.type >= 500 && response.type <= 599)
  {
    printWarning(this, "Got a Server Failure Response.");
    shouldLive_ = false;
  }
  else if (response.type >= 600 && response.type <= 699)
  {
    printWarning(this, "Got a Global Failure Response.");
    if (response.message->cSeq.method == SIP_INVITE)
    {
      if (response.type == SIP_DECLINE)
      {
        transactionUser_->endCall(sessionID_);
      }
    }
    shouldLive_ = false;
  }
}


void SIPSingleCall::transmitRequest(SIPRequest& request, QVariant &content)
{
  Q_UNUSED(content);
  emit sendDialogRequest(sessionID_, request);
}


void SIPSingleCall::transmitResponse(SIPResponse& response, QVariant& content)
{
  emit sendResponse(sessionID_, response);
}


void SIPSingleCall::processFailure(QString message)
{
  transactionUser_->failure(sessionID_, message);
  shouldLive_ = false;
}
