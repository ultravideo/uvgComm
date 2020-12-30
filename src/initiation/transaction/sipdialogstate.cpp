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
  sessionState_(false)
{}


void SIPDialogState::createNewDialog(NameAddr &remote, QString localAddress,
                                     bool registered)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState", "Creating a new dialog.");
  initDialog();

  remoteURI_ = remote;
  requestUri_ = remote.uri;
  if(!registered)
  {
    printDebug(DEBUG_NORMAL, "SIPDialogState", "Setting peer-to-peer address.");
    localURI_.uri.hostport.host = localAddress;
  }
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


void SIPDialogState::createDialogFromINVITE(std::shared_ptr<SIPMessageHeader> &inMessage,
                                            QString hostName)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState",
             "Creating a dialog from incoming INVITE.");
  Q_ASSERT(callID_ == "");
  Q_ASSERT(inMessage);

  if(callID_ != "")
  {
    if(correctRequestDialog(inMessage, SIP_INVITE, inMessage->cSeq.cSeq))
    {
      printDebug(DEBUG_PROGRAM_ERROR, "SIPDialogState",
                 "Re-INVITE should be processed differently.");
      return;
    }
    else
    {
      printDebug(DEBUG_PEER_ERROR, "SIPDialogState",
                 "Got a request not belonging to this dialog.");
      return;
    }
  }

  setDialog(inMessage->callID); // TODO: port

  remoteURI_ = inMessage->from.address;

  // in future we will address our requests to their contact address
  requestUri_ = inMessage->contact.first().address.uri;

  localURI_.uri.hostport.host = hostName;

  remoteTag_ = inMessage->from.tag;
  if(remoteTag_ == "")
  {
    printDebug(DEBUG_PEER_ERROR, "SIPDialogState",
               "They did not provide their tag in INVITE!");
    // TODO: send an error response.
  }

  remoteCSeq_ = inMessage->cSeq.cSeq;

  // Set the request to tag to local tag value so when sending the response
  // it is already there.
  if(inMessage->to.tag == "")
  {
    inMessage->to.tag = localTag_;
  }

  route_ = inMessage->recordRoutes;

  qDebug() << "Received a dialog creating INVITE. Creating dialog."
           << "CallID: " << callID_ << "OurTag:" << localTag_ << "Cseq:" << localCSeq_;
}


void SIPDialogState::getRequestDialogInfo(SIPRequest &outRequest)
{
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
  }

  outRequest.requestURI = requestUri_;

  if(outRequest.method != SIP_ACK && outRequest.method != SIP_CANCEL)
  {
    ++localCSeq_;
    printDebug(DEBUG_NORMAL, "SIPDialogState",  "Increasing CSeq",
              {"CSeq"}, {QString::number(localCSeq_)});
  }

  outRequest.message->cSeq.cSeq = localCSeq_;

  outRequest.message->from.address = localURI_;
  outRequest.message->from.tag = localTag_;

  outRequest.message->to.address = remoteURI_;
  outRequest.message->to.tag = remoteTag_;

  outRequest.message->callID = callID_;

  outRequest.message->routes = route_;
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
  if((inMessage->to.tag == localTag_) && inMessage->from.tag == remoteTag_ &&
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
  if(inMessage->from.tag == localTag_ && (inMessage->to.tag == remoteTag_ || remoteTag_ == "") &&
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
      remoteTag_ = inMessage->to.tag;
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
  localURI_.uri.hostport.host = settings.value("sip/ServerAddress").toString();
  localURI_.uri.type = DEFAULT_SIP_TYPE;
  localURI_.uri.hostport.port = 0; // port is added later if needed

  if(localURI_.uri.userinfo.user.isEmpty())
  {
    localURI_.uri.userinfo.user = "anonymous";
  }
}


void SIPDialogState::setRoute(QList<SIPRouteLocation>& route)
{
  route_ = route;
}
