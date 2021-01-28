#include "sipserver.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

SIPServer::SIPServer():
  sessionID_(0),
  receivedRequest_(nullptr),
  transactionUser_(nullptr),
  shouldLive_(true)
{}

void SIPServer::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  transactionUser_ = tu;
  sessionID_ = sessionID;
}


void SIPServer::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  Q_UNUSED(content)
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
               "SIP Server transaction not initialized.");
    shouldLive_ = false;
    return;
  }

  if((receivedRequest_ == nullptr && request.method != SIP_ACK) ||
     request.method == SIP_BYE)
  {
    receivedRequest_ = std::shared_ptr<SIPRequest> (new SIPRequest);
    *receivedRequest_ = request;
  }
  else if (request.method != SIP_ACK && request.method != SIP_CANCEL)
  {
    printDebug(DEBUG_PEER_ERROR, "SIP Server Transaction",
               "They sent us a new SIP request even though we have the old one still saved.",
                {"SessionID"}, {QString::number(sessionID_)});
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
      responseSender(SIP_RINGING);
    }
    else
    {
      responseSender(SIP_OK);
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
    responseSender(SIP_OK);

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
    printPeerError(this, "REGISTER-method detected at server. Why?");
    responseSender(SIP_NOT_ALLOWED);
    break;
  }
  default:
  {
    printUnimplemented(this, "Unsupported request type received");
    responseSender(SIP_NOT_ALLOWED);
    break;
  }
  }
  shouldLive_ = true;
  return;
}


void SIPServer::respondOK()
{
  responseSender(SIP_OK);
}


void SIPServer::respondDECLINE()
{
  responseSender(SIP_DECLINE);
}


void SIPServer::responseSender(SIPResponseStatus type)
{
  Q_ASSERT(receivedRequest_ != nullptr);

  if(receivedRequest_ == nullptr)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
               "The received request was not set before trying to use it!");
    return;
  }

  printNormal(this, "Initiate sending of a dialog response");

  SIPResponse response;
  response.type = type;
  response.sipVersion = SIP_VERSION;

  copyResponseDetails(receivedRequest_->message, response.message);
  response.message->maxForwards = nullptr; // no max-forwards in responses

  response.message->contentLength = 0;
  response.message->contentType = MT_NONE;

  int responseCode = type;
  if(responseCode >= 200)
  {
    printDebug(DEBUG_NORMAL, this,
               "Sending a final response. Deleting request details.",
               {"SessionID", "Code", "Cseq"},
               {QString::number(sessionID_), QString::number(responseCode),
                QString::number(receivedRequest_->message->cSeq.cSeq)});

    // reset the request since we have responded to it
    receivedRequest_ = nullptr;
  }

  emit sendResponse(sessionID_, response);
}


void SIPServer::copyResponseDetails(std::shared_ptr<SIPMessageHeader>& inMessage,
                                    std::shared_ptr<SIPMessageHeader>& copy)
{
  Q_ASSERT(inMessage);
  Q_ASSERT(inMessage->from.tagParameter != "");
  Q_ASSERT(inMessage->to.tagParameter != "");
  copy = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader());
  // Which fields to copy are listed in section 8.2.6.2 of RFC 3621

  // Call-ID field
  copy->callID = inMessage->callID;

  // CSeq
  copy->cSeq = inMessage->cSeq;

  // from-field
  copy->from = inMessage->from;

  // To field, expect if To tag is missing, in which case it should be added
  // To tag is added in dialog when checking the first request.
  copy->to = inMessage->to;

  // Via- fields in same order
  copy->vias = inMessage->vias;

  copy->recordRoutes = inMessage->recordRoutes;
}


bool SIPServer::isCANCELYours(SIPRequest& cancel)
{
  return receivedRequest_ != nullptr &&
      !receivedRequest_->message->vias.empty() &&
      !cancel.message->vias.empty() &&
      receivedRequest_->message->vias.first().branch == cancel.message->vias.first().branch &&
      equalURIs(receivedRequest_->requestURI, cancel.requestURI) &&
      receivedRequest_->message->callID == cancel.message->callID &&
      receivedRequest_->message->cSeq.cSeq == cancel.message->cSeq.cSeq &&
      equalToFrom(receivedRequest_->message->from, cancel.message->from) &&
      equalToFrom(receivedRequest_->message->to, cancel.message->to);
}


bool SIPServer::equalURIs(SIP_URI& first, SIP_URI& second)
{
  return first.type == second.type &&
      first.hostport.host == second.hostport.host &&
      first.hostport.port == second.hostport.port &&
      first.userinfo.user == second.userinfo.user;
}


bool SIPServer::equalToFrom(ToFrom& first, ToFrom& second)
{
  return first.address.realname == second.address.realname &&
      equalURIs(first.address.uri, second.address.uri);
}
