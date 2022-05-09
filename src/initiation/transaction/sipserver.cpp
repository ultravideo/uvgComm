#include "sipserver.h"

#include "initiation/transaction/sipdialogstate.h"

#include "common.h"
#include "logger.h"

#include <QVariant>

SIPServer::SIPServer():
  shouldLive_(true),
  receivedRequest_(nullptr)
{}


void SIPServer::processIncomingRequest(SIPRequest& request, QVariant& content,
                                       SIPResponseStatus generatedResponse)
{
  Logger::getLogger()->printNormal(this, "Processing incoming request");

  if (request.method == SIP_CANCEL && !isCANCELYours(request))
  {
    Logger::getLogger()->printError(this, "Received invalid CANCEL request");
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
    Logger::getLogger()->printPeerError(this, "New request when previous transaction "
                                              "has not been completed. Ignoring...");
    return;
  }

  switch (request.method)
  {
    case SIP_INVITE:
    {
      // user deals with INVITE responses
      break;
    }
    case SIP_ACK:
    {
      // ACK has no response
      break;
    }
    case SIP_BYE:
    {
      createResponse(SIP_OK);
      shouldLive_ = false;
      break;
    }
    case SIP_CANCEL:
    {
      createResponse(SIP_REQUEST_TERMINATED);
      shouldLive_ = false;
      break;
    }
    case SIP_REGISTER:
    {
      // REGISTER not implemented
      createResponse(SIP_NOT_ALLOWED);
      return;
    }
    case SIP_OPTIONS:
    {
      // OPTIONS not implemented
      createResponse(SIP_NOT_ALLOWED);
      return;
    }
    default:
    {
      // Unknown request type
      createResponse(SIP_NOT_ALLOWED);
      return;
    }
  }

  // send the request to callbacks
  emit incomingRequest(request, content, generatedResponse);
}


void SIPServer::respond_INVITE_RINGING()
{
  if (receivedRequest_ == nullptr || receivedRequest_->message->cSeq.method != SIP_INVITE)
  {
    Logger::getLogger()->printProgramError(this, "No INVITE found for OK");
    return;
  }

  createResponse(SIP_RINGING);
}


void SIPServer::respond_INVITE_OK()
{
  if (receivedRequest_ == nullptr || receivedRequest_->message->cSeq.method != SIP_INVITE)
  {
    Logger::getLogger()->printProgramError(this, "No INVITE found for OK");
    return;
  }

  createResponse(SIP_OK);
}


void SIPServer::respond_INVITE_DECLINE()
{
  if (receivedRequest_ == nullptr || receivedRequest_->message->cSeq.method != SIP_INVITE)
  {
    Logger::getLogger()->printProgramError(this, "No INVITE found for DECLINE");
    return;
  }

  createResponse(SIP_DECLINE);
}


bool SIPServer::doesCANCELMatchRequest(SIPRequest &request) const
{
  if (receivedRequest_ == nullptr)
  {
    Logger::getLogger()->printNormal(this, "No previous request when evaluating CANCEL");
  }
  else if (!(receivedRequest_->requestURI == request.requestURI))
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Different request URI in CANCEL",
                                     {"Existing request uri user", "Request uri user",
                                      "Existing request uri host", "Request uri host"},
                                     {receivedRequest_->requestURI.userinfo.user, request.requestURI.userinfo.user,
                                      receivedRequest_->requestURI.hostport.host, request.requestURI.hostport.host});
  }
  else if (receivedRequest_->message->callID != request.message->callID)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Different Call-ID in CANCEL",
                                     {"Existing request Call-ID", "Request Call-ID"},
                                     {receivedRequest_->message->callID, request.message->callID});
  }
  else if (!(receivedRequest_->message->to.address.uri == request.message->to.address.uri))
  {
    Logger::getLogger()->printNormal(this, "Different to address in CANCEL");
  }
  else if (receivedRequest_->message->to.tagParameter != request.message->to.tagParameter)
  {
    Logger::getLogger()->printNormal(this, "Different to parameters in CANCEL");
  }
  else if (!(receivedRequest_->message->from.address.uri == request.message->from.address.uri))
  {
    Logger::getLogger()->printNormal(this, "Different from address in CANCEL");
  }
  else if (receivedRequest_->message->from.tagParameter != request.message->from.tagParameter)
  {
    Logger::getLogger()->printNormal(this, "Different from parameters in CANCEL");
  }
  else if (receivedRequest_->message->cSeq.cSeq != request.message->cSeq.cSeq)
  {
    Logger::getLogger()->printNormal(this, "Different cSeq in CANCEL");
  }

  // see section 9.1 of RFC 3261
  return receivedRequest_                          != nullptr &&
      receivedRequest_->requestURI                 == request.requestURI &&
      receivedRequest_->message->callID            == request.message->callID &&
      receivedRequest_->message->to.address.uri    == request.message->to.address.uri &&
      receivedRequest_->message->to.tagParameter   == request.message->to.tagParameter &&
      receivedRequest_->message->from.address.uri  == request.message->from.address.uri &&
      receivedRequest_->message->from.tagParameter == request.message->from.tagParameter &&
      receivedRequest_->message->cSeq.cSeq         == request.message->cSeq.cSeq;
}


void SIPServer::createResponse(SIPResponseStatus status)
{
  SIPResponse response;
  response.type = status;
  response.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);

  if(receivedRequest_ == nullptr)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "We are trying to respond when we have not received a request!");
    return;
  }

  Logger::getLogger()->printNormal(this, "Initiate sending of a dialog response");
  response.sipVersion = SIP_VERSION;

  copyResponseDetails(receivedRequest_->message, response.message);
  response.message->maxForwards = nullptr; // no max-forwards in responses

  response.message->contentLength = 0;
  response.message->contentType = MT_NONE;

  if(response.type >= 200)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                   "Sending a final response. Deleting request details.",
                                   {"Code", "Cseq"},
                                   {QString::number(response.type),
                                    QString::number(receivedRequest_->message->cSeq.cSeq)});

    // reset the request since we have responded to it
    receivedRequest_ = nullptr;
  }

  QVariant content;
  emit outgoingResponse(response, content);
}


void SIPServer::copyResponseDetails(std::shared_ptr<SIPMessageHeader>& inMessage,
                                    std::shared_ptr<SIPMessageHeader>& copy)
{
  Q_ASSERT(inMessage);
  Q_ASSERT(inMessage->from.tagParameter != "");
  copy = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader());
  // Which fields to copy are listed in section 8.2.6.2 of RFC 3621

  // Call-ID field
  copy->callID = inMessage->callID;

  // CSeq
  copy->cSeq = inMessage->cSeq;

  // from-field
  copy->from = inMessage->from;

  // To field, expect if To tag is missing, in which case it should be added later
  // To tag is the responsibility of SIPDialog
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
