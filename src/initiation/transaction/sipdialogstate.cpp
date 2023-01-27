#include "sipdialogstate.h"

#include "common.h"
#include "logger.h"

#include <QDateTime>
#include <QSettings>

const uint32_t UINT31_MAX = 2147483647;


SIPDialogState::SIPDialogState():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  localCseq_(UINT32_MAX),
  remoteCseq_(UINT32_MAX),
  route_(),
  previousRequest_({SIP_NO_REQUEST, {}, "",nullptr}),
  callActive_(false)
{}


SIPDialogState::~SIPDialogState()
{}


void SIPDialogState::init(NameAddr &local, NameAddr &remote, bool createDialog)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "SIPDialogState", "Creating a new dialog.");

  localURI_ = local;
  remoteURI_ = remote;

  remoteTarget_ = remote.uri;

  if (createDialog)
  {
    initDialog();
  }
}


void SIPDialogState::createServerConnection(NameAddr &local, SIP_URI requestURI)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "SIPDialogState", "Creating a Server dialog.");

  init(local, local, true);
  remoteTarget_ = requestURI; // server connection has different request uri from to
}


bool SIPDialogState::correctRequestDialog(QString callID, QString toTag, QString fromTag)
{
  Q_ASSERT(callID_ != "");
  if(callID_ == "")
  {
    Logger::getLogger()->printWarning(this, "The SIP dialog has not been initialized, but it is used");
    return false;
  }

  // TODO: For backwards compability, this should be prepared for missing To-tag (or was it from-tag) (RFC3261).

  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  return toTag == localTag_ && fromTag == remoteTag_ && callID == callID_;
}


bool SIPDialogState::correctResponseDialog(QString callID, QString toTag, QString fromTag)
{
  // For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).

  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  // we may not yet have their tag in which case we don't check it
  return (fromTag == localTag_ && (toTag == remoteTag_ || remoteTag_ == "") &&
          callID == callID_);
}


void SIPDialogState::initDialog()
{
  localTag_ = generateRandomString(TAG_LENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(localURI_.uri.hostport.host != "")
  {
    callID_ += "@" + localURI_.uri.hostport.host;
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Local dialog created",
             {"Call-ID", "Tag"}, {callID_, localTag_});
}


void SIPDialogState::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Q_UNUSED(content)

  Q_ASSERT(localURI_.uri.userinfo.user != "" && localURI_.uri.hostport.host != "");
  Q_ASSERT(remoteURI_.uri.userinfo.user != "" && remoteURI_.uri.hostport.host != "");

  Logger::getLogger()->printNormal(this, "Processing outgoing request");

  if(localURI_.uri.userinfo.user == "" || localURI_.uri.hostport.host == "" ||
     remoteURI_.uri.userinfo.user == "" || remoteURI_.uri.hostport.host == "")
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SIPDialogState",
               "The dialog state info has not been set, but we are using it.",
                {"username", "host", "remote username", "remote host"},
                {localURI_.uri.userinfo.user, localURI_.uri.hostport.host,
                 remoteURI_.uri.userinfo.user, remoteURI_.uri.hostport.host});
    return;
  }

  if (request.method == SIP_CANCEL)
  {
    if (localCseq_ == UINT32_MAX)
    {
      Logger::getLogger()->printProgramError(this, "Local CSeq does not exist in CANCEL");
      return;
    }

    // see section 9.1 of RFC 3261 for cancelling request

    if (previousRequest_.method != SIP_INVITE)
    {
      Logger::getLogger()->printProgramWarning(this, "We are trying to CANCEL a non-INVITE request!");
      return;
    }

    /* The Request-URI, Call-ID, To, the numeric part of CSeq, and From header
       fields, including tags must be identical */

    request.requestURI = previousRequest_.requestURI;
    request.message->callID = previousRequest_.message->callID;

    // cseq method is still CANCEL even though cSeq itself is from cancelled request
    request.message->cSeq.cSeq = previousRequest_.message->cSeq.cSeq;

    request.message->from = previousRequest_.message->from;
    request.message->to = previousRequest_.message->to;
  }
  else // Not a CANCEL but an actual request from us
  {
    // routing of this request
    if (!route_.empty())
    {
      bool foundLR = false;

      for (auto& parameter : route_.first().address.uri.uri_parameters)
      {
        if (parameter.name == "lr")
        {
          foundLR = true;
        }
      }

      if (foundLR)
      {
        request.requestURI = remoteTarget_;
        request.message->routes = route_;
      }
      else
      {
        request.requestURI = route_.first().address.uri;
        // TODO: All parameters not allowed in request-URI

        QList<SIPRouteLocation> routes = route_;
        routes.pop_front();

        route_.push_back({{{}, remoteTarget_}, {}});
        request.message->routes = routes;
      }
    }
    else
    {
      request.requestURI = remoteTarget_;
    }

    // init local cseq if it does not exist
    if (localCseq_ == UINT32_MAX)
    {
      if (request.method == SIP_ACK)
      {
        Logger::getLogger()->printProgramError(this, "No cseq in ACK");
        return;
      }

      localCseq_ = initialCSeqNumber();
      Logger::getLogger()->printNormal(this, "Initializing local CSeq with first request", "CSeq", QString::number(localCseq_));
    }
    else
    {
      if(request.method != SIP_ACK && request.method != SIP_CANCEL)
      {
        ++localCseq_;
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SIPDialogState",  "Increasing CSeq",
                   {"CSeq"}, {QString::number(localCseq_)});
      }
    }

    if (request.message->cSeq.method == SIP_ACK && request.method == SIP_ACK)
    {
      callActive_ = true;
    }
    else if (request.method == SIP_BYE)
    {
      callActive_ = false;
    }

    request.message->cSeq.cSeq = localCseq_;

    request.message->from.address = localURI_;
    request.message->from.tagParameter = localTag_;

    request.message->to.address = remoteURI_;
    request.message->to.tagParameter = remoteTag_;

    request.message->callID = callID_;

    previousRequest_ = request;
  }

  emit outgoingRequest(request, content);
}


void SIPDialogState::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  // see section 8.2.6.2 of RFC 3261
  if (response.message->to.tagParameter == "")
  {
    // we set our tag only at this point so we can correctly recognize when CANCEL comes without our tag
    localTag_ = generateRandomString(TAG_LENGTH);
    response.message->to.tagParameter = localTag_;

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Generated our tag to response. Creating dialog.",
               {"Call-ID", "Local Tag"}, {callID_, localTag_});
  }

  emit outgoingResponse(response, content);
}


void SIPDialogState::processIncomingRequest(SIPRequest& request, QVariant& content,
                                            SIPResponseStatus generatedResponse)
{
  Q_UNUSED(content)
  Q_ASSERT(request.message);

  Logger::getLogger()->printNormal(this, "Processing incoming request");

  if (localURI_.uri.hostport.host == "" ||
      remoteURI_.uri.hostport.host == "")
  {
    Logger::getLogger()->printProgramError(this, "Dialog state not initialized");
    return;
  }

  if(request.message->from.tagParameter == "")
  {
    Logger::getLogger()->printDebug(DEBUG_PEER_ERROR, this,
               "They did not provide their tag!");
    // TODO: send an error response.
    return;
  }

  if (callID_ == "" && request.method == SIP_INVITE)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SIPDialogState",
               "Creating a dialog from incoming INVITE.");

    callID_ = request.message->callID;
    remoteTag_ = request.message->from.tagParameter;
  }
  else
  {
    // we haven't yet received their first request
    if (remoteCseq_ != UINT32_MAX && request.message->cSeq.cSeq <= UINT31_MAX)
    {
      // The request cseq should be larger by one than previous.
      if(request.message->cSeq.cSeq != remoteCseq_ + 1 &&
         ((request.method != SIP_ACK && request.method != SIP_CANCEL) ||
          request.message->cSeq.cSeq != remoteCseq_))
      {
        Logger::getLogger()->printPeerError(this, "Invalid CSeq in received request");
        // TODO: if remote cseq in message is lower than remote cseq, send 500
        return;
      }
    }
  }

  // in future we will address our requests to their contact address
  if (!request.message->contact.empty())
  {
    remoteTarget_ = request.message->contact.first().address.uri;
  }

  if (!request.message->recordRoutes.empty())
  {
    route_ = request.message->recordRoutes;
  }

  remoteCseq_ = request.message->cSeq.cSeq;

  if (request.message->cSeq.method == SIP_ACK && request.method == SIP_ACK)
  {
    callActive_ = true;
  }
  else if (request.method == SIP_BYE)
  {
    callActive_ = false;
  }

  emit incomingRequest(request, content, generatedResponse);
}


void SIPDialogState::processIncomingResponse(SIPResponse& response, QVariant& content,
                                             bool retryRequest)
{
  Q_UNUSED(content)

  // The response cseq should be the same as our cseq
  if(response.message->cSeq.cSeq != localCseq_)
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, this,
                                    "The CSeq in incoming response was not same as request",
                                    {"Our CSeq", "Response CSeq"},
                                    {QString::number(localCseq_),
                                     QString::number(response.message->cSeq.cSeq)});
    return;
  }

  // set the correct request URI from their contact field
  if (response.type == SIP_OK &&
      response.message->cSeq.method == SIP_INVITE)
  {
    Logger::getLogger()->printNormal(this, "We don't yet have their remote Tag. "
                                           "Using the one in INVITE OK response.");
    remoteTag_ = response.message->to.tagParameter;
  }

  if (!response.message->contact.empty() &&
      response.message->cSeq.method != SIP_REGISTER)
  {
    remoteTarget_ = response.message->contact.first().address.uri;
  }

  if (!response.message->recordRoutes.empty())
  {
    route_ = response.message->recordRoutes;
  }

  emit incomingResponse(response, content, retryRequest);
}


uint32_t SIPDialogState::initialCSeqNumber()
{
  // cseq start value. For example 31-bits of 32-bit clock
  // 31-bits is the maximum for initial value
  return QDateTime::currentSecsSinceEpoch()%UINT31_MAX;
}
