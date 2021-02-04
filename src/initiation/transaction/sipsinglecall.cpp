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

  connect(client_.get(), &SIPClient::receivedResponse,
          this,         &SIPSingleCall::processResponse);

  connect(client_.get(), &SIPClient::outgoingRequest,
          this,         &SIPSingleCall::transmitRequest);

  connect(client_.get(), &SIPClient::failure,
          this,         &SIPSingleCall::processFailure);

  server_ = server;

  connect(server_.get(), &SIPServer::receivedRequest,
          this,          &SIPSingleCall::processRequest);

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


void SIPSingleCall::processRequest(SIPRequestMethod request, QString &fromAddress)
{
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
               "SIP Server transaction not initialized.");
    shouldLive_ = false;
    return;
  }

  switch(request)
  {
  case SIP_INVITE:
  {
    if (!transactionUser_->incomingCall(sessionID_, fromAddress))
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


void SIPSingleCall::processResponse(SIPRequestMethod originalRequest,
                                    SIPResponseStatus status)
{
  if (status >= 100 && status <= 299)
  {
    // process anything that is not a failure and may cause a new request to be sent.
    // this must be done after the SIPClientTransaction processResponse, because that checks our
    // current active transaction and may modify it.
    if(originalRequest == SIP_INVITE)
    {
      if(status == SIP_RINGING)
      {
        transactionUser_->callRinging(sessionID_);
      }
      else if(status == SIP_OK)
      {
        transactionUser_->peerAccepted(sessionID_);

        client_->sendRequest(SIP_ACK);
        transactionUser_->callNegotiated(sessionID_);
      }
    }
    else if (originalRequest == SIP_BYE)
    {
      transactionUser_->endCall(sessionID_);
    }
    shouldLive_ = true;
  }
  else if (status >= 300 && status <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    printWarning(this, "Got a Redirection Response.");
    shouldLive_ = false;
  }
  else if (status >= 400 && status <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    printWarning(this, "Got a Failure Response.");
    shouldLive_ = false;
  }
  else if (status >= 500 && status <= 599)
  {
    printWarning(this, "Got a Server Failure Response.");
    shouldLive_ = false;
  }
  else if (status >= 600 && status <= 699)
  {
    printWarning(this, "Got a Global Failure Response.");
    if (originalRequest == SIP_INVITE)
    {
      if (status == SIP_DECLINE)
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
