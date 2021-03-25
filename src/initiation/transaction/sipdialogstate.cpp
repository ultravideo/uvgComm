#include "sipdialogstate.h"

#include "initiation/siptransactionuser.h"

#include "common.h"
#include "settingskeys.h"

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


void SIPDialogState::createNewDialog(SIP_URI remoteURI, QString localAddress,
                                     bool registered)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState", "Creating a new dialog.");
  initDialog();

  remoteURI_ = remoteURI;
  requestUri_ = remoteURI;
  if(!registered)
  {
    printDebug(DEBUG_NORMAL, "SIPDialogState", "Setting peer-to-peer address.");
    localURI_.host = localAddress;
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


void SIPDialogState::createDialogFromINVITE(std::shared_ptr<SIPMessageInfo> &inMessage,
                                            QString hostName)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState",
             "Creating a dialog from incoming INVITE.");
  Q_ASSERT(callID_ == "");
  Q_ASSERT(inMessage);
  Q_ASSERT(inMessage->dialog);

  if(callID_ != "")
  {
    if(correctRequestDialog(inMessage->dialog, SIP_INVITE, inMessage->cSeq))
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

  setDialog(inMessage->dialog->callID); // TODO: port

  remoteURI_ = inMessage->from;

  // in future we will address our requests to their contact address
  requestUri_ = inMessage->contact;

  localURI_.host = hostName;

  remoteTag_ = inMessage->dialog->fromTag;
  if(remoteTag_ == "")
  {
    printDebug(DEBUG_PEER_ERROR, "SIPDialogState",
               "They did not provide their tag in INVITE!");
    // TODO: send an error response.
  }

  remoteCSeq_ = inMessage->cSeq;

   // set the request to tag to local tag value so when sending the response it is already there.
  if(inMessage->dialog->toTag == "")
  {
    inMessage->dialog->toTag = localTag_;
  }

  route_ = inMessage->recordRoutes;

  qDebug() << "Received a dialog creating INVITE. Creating dialog."
           << "CallID: " << callID_ << "OurTag:" << localTag_ << "Cseq:" << localCSeq_;
}


void SIPDialogState::getRequestDialogInfo(SIPRequest &outRequest)
{
  Q_ASSERT(localURI_.username != "" && localURI_.host != "");
  Q_ASSERT(remoteURI_.username != "" && remoteURI_.host != "");

  if(localURI_.username == "" || localURI_.host == "" ||
     remoteURI_.username == "" || remoteURI_.host == "")
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPDialogState", 
               "The dialog state info has not been set, but we are using it.",
                {"username", "host", "remote username", "remote host"},
                {localURI_.username, localURI_.host, remoteURI_.username, remoteURI_.host});
  }

  outRequest.requestURI = requestUri_;

  if(outRequest.type != SIP_ACK && outRequest.type != SIP_CANCEL)
  {
    ++localCSeq_;
    printDebug(DEBUG_NORMAL, "SIPDialogState",  "Increasing CSeq",
              {"CSeq"}, {QString::number(localCSeq_)});
  }

  outRequest.message->cSeq = localCSeq_;
  outRequest.message->from = localURI_;
  outRequest.message->to = remoteURI_;

  outRequest.message->routes = route_;

  // SIPDialogInfo format: toTag, fromTag, CallID
  outRequest.message->dialog
      = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{remoteTag_, localTag_, callID_});
}


bool SIPDialogState::correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog,
                                          RequestType type, uint32_t remoteCSeq)
{
  Q_ASSERT(callID_ != "");
  if(callID_ == "")
  {
    qDebug() << "WARNING: The SIP dialog has not been initialized, but it is used";
    return false;
  }

  // TODO: For backwards compability, this should be prepared for missing To-tag (or was it from-tag) (RFC3261).

  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if((dialog->toTag == localTag_) && dialog->fromTag == remoteTag_ &&
     ( dialog->callID == callID_))
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


bool SIPDialogState::correctResponseDialog(std::shared_ptr<SIPDialogInfo> dialog,
                                           uint32_t messageCSeq, bool recordToTag)
{
  // For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).
  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if(dialog->fromTag == localTag_ && (dialog->toTag == remoteTag_ || remoteTag_ == "") &&
     dialog->callID == callID_)
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
      remoteTag_ = dialog->toTag;
    }

    return true;
  }
  return false;
}


void SIPDialogState::initDialog()
{
  initLocalURI();

  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(localURI_.host != "")
  {
    callID_ += "@" + localURI_.host;
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
    localTag_ = generateRandomString(TAGLENGTH);
  }
}


void SIPDialogState::initLocalURI()
{
  // init stuff from the settings
  QSettings settings(settingsFile, settingsFileFormat);

  localURI_.realname = settings.value(SettingsKey::localRealname).toString();
  localURI_.username = getLocalUsername();
  localURI_.host = settings.value(SettingsKey::sipServerAddress).toString();
  localURI_.connectionType = TRANSPORTTYPE;
  localURI_.port = 0; // port is added later if needed

  if(localURI_.username.isEmpty())
  {
    localURI_.username = "anonymous";
  }
}


void SIPDialogState::setRoute(QList<SIP_URI>& route)
{
  route_ = route;
}
