#include "sipdialogstate.h"

#include "initiation/siptransactionuser.h"

#include "common.h"

#include <QDateTime>
#include <QDebug>
#include <QSettings>


SIPDialogState::SIPDialogState():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  // cseq start value. For example 31-bits of 32-bit clock
  localCSeq_(QDateTime::currentSecsSinceEpoch()%2147483647),
  remoteCSeq_(0),
  route_(),
  previousRequest_({SIP_NO_REQUEST, {}, "",nullptr})
{}


SIPDialogState::~SIPDialogState()
{}


void SIPDialogState::setLocalHost(QString localAddress)
{
  printNormal(this, "Setting Dialog local host.", {"Address"}, {localAddress});
  localURI_.uri.hostport.host = localAddress;
}


void SIPDialogState::createNewDialog(NameAddr &remote)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState", "Creating a new dialog.");
  initDialog();

  remoteURI_ = remote;
  requestUri_ = remote.uri;
}


void SIPDialogState::createServerConnection(SIP_URI requestURI)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState",
             "Creating a SIP Server dialog.");
  initDialog();

  remoteURI_ = localURI_;
  requestUri_ = requestURI; // server has different request uri from remote
  localCSeq_ = 0; //
}


bool SIPDialogState::correctRequestDialog(std::shared_ptr<SIPMessageHeader> &inMessage,
                                          SIPRequestMethod type, uint32_t remoteCSeq)
{
  Q_ASSERT(callID_ != "");
  if(callID_ == "")
  {
    qDebug() << "WARNING: The SIP dialog has not been initialized, but it is used";
    return false;
  }

  // TODO: For backwards compability, this should be prepared for missing To-tag (or was it from-tag) (RFC3261).

  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if((inMessage->to.tagParameter == localTag_) && inMessage->from.tagParameter == remoteTag_ &&
     ( inMessage->callID == callID_))
  {
    // The request cseq should be larger than our remotecseq.
    if(remoteCSeq <= remoteCSeq_ && type != SIP_ACK && type != SIP_CANCEL)
    {
      qDebug() << "PEER_ERROR:"
               << "Their request Cseq was smaller than their previous cseq which is not permitted!";
      // TODO: if remote cseq in message is lower than remote cseq, send 500
      return false;
    }

    remoteCSeq_ = remoteCSeq;
    return true;
  }
  return false;
}


bool SIPDialogState::correctResponseDialog(std::shared_ptr<SIPMessageHeader> &inMessage,
                                           uint32_t messageCSeq, bool recordToTag)
{
  // For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).
  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if(inMessage->from.tagParameter == localTag_ && (inMessage->to.tagParameter == remoteTag_ || remoteTag_ == "") &&
     inMessage->callID == callID_)
  {
    // The response cseq should be the same as our cseq
    if(messageCSeq != localCSeq_)
    {
      qDebug() << "PEER_ERROR:" << "The response CSeq was not the same as our previous request!"
               << messageCSeq << "vs local" << localCSeq_;
      // TODO: if remote cseq in message is lower than remote cseq, send 500
      return false;
    }

    if(remoteTag_ == "" && recordToTag)
    {
      qDebug() << "We don't yet have their remote Tag. Using the one in response.";
      remoteTag_ = inMessage->to.tagParameter;
    }

    return true;
  }
  return false;
}


void SIPDialogState::initDialog()
{
  initLocalURI();

  localTag_ = generateRandomString(TAG_LENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(localURI_.uri.hostport.host != "")
  {
    callID_ += "@" + localURI_.uri.hostport.host;
  }

  qDebug() << "Local dialog created. CallID: " << callID_
           << "Tag:" << localTag_ << "Cseq:" << localCSeq_;
}


void SIPDialogState::setDialog(QString callID)
{
  initLocalURI();
  callID_ = callID;

  if(localTag_ == "")
  {
    localTag_ = generateRandomString(TAG_LENGTH);
  }
}


void SIPDialogState::initLocalURI()
{
  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  localURI_.realname = settings.value("local/Name").toString();
  localURI_.uri.userinfo.user = getLocalUsername();

  // dont set server address if we have already set peer-to-peer address
  if (localURI_.uri.hostport.host != "")
  {
    localURI_.uri.hostport.host = settings.value("sip/ServerAddress").toString();
  }


  localURI_.uri.type = DEFAULT_SIP_TYPE;
  localURI_.uri.hostport.port = 0; // port is added later if needed

  if(localURI_.uri.userinfo.user.isEmpty())
  {
    localURI_.uri.userinfo.user = "anonymous";
  }
}


void SIPDialogState::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Q_UNUSED(content)

  Q_ASSERT(localURI_.uri.userinfo.user != "" && localURI_.uri.hostport.host != "");
  Q_ASSERT(remoteURI_.uri.userinfo.user != "" && remoteURI_.uri.hostport.host != "");

  if(localURI_.uri.userinfo.user == "" || localURI_.uri.hostport.host == "" ||
     remoteURI_.uri.userinfo.user == "" || remoteURI_.uri.hostport.host == "")
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPDialogState",
               "The dialog state info has not been set, but we are using it.",
                {"username", "host", "remote username", "remote host"},
                {localURI_.uri.userinfo.user, localURI_.uri.hostport.host,
                 remoteURI_.uri.userinfo.user, remoteURI_.uri.hostport.host});
    return;
  }

  if (request.method == SIP_CANCEL)
  {
    if (previousRequest_.method != SIP_INVITE)
    {
      printProgramWarning(this, "Trying to CANCEL a non-INVITE request!");
      return;
    }

    request.requestURI = previousRequest_.requestURI;
    request.message->callID = previousRequest_.message->callID;

    // cseq method is still CANCEL
    request.message->cSeq.cSeq = previousRequest_.message->cSeq.cSeq;

    request.message->from = previousRequest_.message->from;
    request.message->to = previousRequest_.message->to;
  }
  else
  {
    request.requestURI = requestUri_;

    if(request.method != SIP_ACK && request.method != SIP_CANCEL)
    {
      ++localCSeq_;
      printDebug(DEBUG_NORMAL, "SIPDialogState",  "Increasing CSeq",
                 {"CSeq"}, {QString::number(localCSeq_)});
    }

    request.message->cSeq.cSeq = localCSeq_;

    request.message->from.address = localURI_;
    request.message->from.tagParameter = localTag_;

    request.message->to.address = remoteURI_;
    request.message->to.tagParameter = remoteTag_;

    request.message->callID = callID_;

    request.message->routes = route_;

    previousRequest_ = request;
  }
}


void SIPDialogState::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  Q_UNUSED(content)
  Q_ASSERT(request.message);

  if (callID_ == "" && request.method == SIP_INVITE)
  {
    printDebug(DEBUG_NORMAL, "SIPDialogState",
               "Creating a dialog from incoming INVITE.");

    if(request.message->from.tagParameter == "")
    {
      printDebug(DEBUG_PEER_ERROR, "SIPDialogState",
                 "They did not provide their tag in INVITE!");
      // TODO: send an error response.
      return;
    }

    setDialog(request.message->callID); // TODO: port

    remoteURI_ = request.message->from.address;

    // in future we will address our requests to their contact address
    requestUri_ = request.message->contact.first().address.uri;

    remoteTag_ = request.message->from.tagParameter;

    remoteCSeq_ = request.message->cSeq.cSeq;

    // Set the request to tag to local tag value so when sending the response
    // it is already there.
    if(request.message->to.tagParameter == "")
    {
      request.message->to.tagParameter = localTag_;
    }

    route_ = request.message->recordRoutes;

    printDebug(DEBUG_NORMAL, this, "Received a dialog creating INVITE. Creating dialog.",
               {"Call-ID", "Local Tag", "CSeq"}, {callID_, localTag_, QString::number(localCSeq_)});
  }
}


void SIPDialogState::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  Q_UNUSED(content)

  // set the correct request URI from their contact field
  if (response.type == SIP_OK &&
      response.message->cSeq.method == SIP_INVITE &&
      !response.message->contact.empty())
  {
    requestUri_ = {response.message->contact.first().address.uri};
  }

  if (!response.message->recordRoutes.empty())
  {
    route_ = response.message->recordRoutes;
  }
}
