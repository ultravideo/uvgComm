#include "sipdialogstate.h"
#include "common.h"

#include "siptransactionuser.h"

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
  secure_(false)
{}

void SIPDialogState::createDialog(SIP_URI remoteURI)
{
  init(remoteURI);

  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(localUri_.host != "")
  {
    callID_ += "@" + localUri_.host;
  }

  qDebug() << "Local dialog created. CallID: " << callID_ << "Tag:" << localTag_ << "Cseq:" << localCSeq_;
}


void SIPDialogState::processFirstINVITE(std::shared_ptr<SIPMessageInfo> &inMessage)
{
  qDebug() << "Initializing SIP dialog with incoming INVITE.";
  Q_ASSERT(callID_ == "");
  Q_ASSERT(inMessage);
  Q_ASSERT(inMessage->dialog);

  init(inMessage->from);

  if(callID_ != "")
  {
    if(correctRequestDialog(inMessage->dialog, INVITE, inMessage->cSeq))
    {
      qDebug() << "Dialog: Got a Re-INVITE";
      return;
    }
    else
    {
      qDebug() << "PEER_ERROR: Got a request not belonging to this dialog";
    }
  }

  remoteTag_ = inMessage->dialog->fromTag;
  if(remoteTag_ == "")
  {
    qDebug() << "PEER_ERROR: They did not provide their tag in INVITE!";
  }

  remoteCSeq_ = inMessage->cSeq;

  if(localTag_ == "")
  {
    localTag_ = generateRandomString(TAGLENGTH);
  }

   // set the request to tag to local tag value so when sending the response it is already there.
  if(inMessage->dialog->toTag == "")
  {
    inMessage->dialog->toTag = localTag_;
  }

  callID_ = inMessage->dialog->callID;

  qDebug() << "Received a dialog creating INVITE. Creating dialog."
           << "CallID: " << callID_ << "OurTag:" << localTag_ << "Cseq:" << localCSeq_;
}

void SIPDialogState::init(SIP_URI remoteURI)
{
  remoteUri_ = remoteURI;

  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  localUri_.realname = settings.value("local/Name").toString();
  localUri_.username = settings.value("local/Username").toString();
  localUri_.host = settings.value("sip/ServerAddress").toString();

  if(localUri_.username.isEmpty())
  {
    localUri_.username = "anonymous";
  }

  // TODO: Get server URI from settings
  localUri_.host = "";
}


void SIPDialogState::getRequestDialogInfo(RequestType type, QString localAddress,
                                     std::shared_ptr<SIPMessageInfo>& outMessage)
{
  Q_ASSERT(localUri_.username != "" && localUri_.host != "");
  Q_ASSERT(remoteUri_.username != "" && remoteUri_.host != "");

  if(localUri_.username == "" || localUri_.host == "" ||
     remoteUri_.username == "" || remoteUri_.host == "")
  {
    qDebug() << "ERROR: The dialog state info has not been set, but we are using it." <<
                "username:" << localUri_.username << "host" << localUri_.host <<
                "remote username:" << remoteUri_.username << "host:" << remoteUri_.host;
  }


  if(type != ACK && type != CANCEL)
  {
    ++localCSeq_;
  }

  outMessage->cSeq = localCSeq_;
  outMessage->from = localUri_;
  outMessage->to = remoteUri_;
  outMessage->contact = localUri_;
  outMessage->senderReplyAddress.push_back(getLocalVia(localAddress));

  // SIPDialogInfo format: toTag, fromTag, CallID
  outMessage->dialog
      = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{remoteTag_, localTag_, callID_});
}

ViaInfo SIPDialogState::getLocalVia(QString localAddress)
{
  return ViaInfo{TCP, "2.0", localAddress, QString("z9hG4bK" + generateRandomString(BRANCHLENGTH))};
}

bool SIPDialogState::correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog, RequestType type, uint32_t remoteCSeq)
{
  Q_ASSERT(callID_ != "");
  if(callID_ == "")
  {
    qDebug() << "WARNING: The SIP dialog has not been initialized, but it is used";
    return false;
  }

  // For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).
  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if((dialog->toTag == localTag_) && dialog->fromTag == remoteTag_ &&
     ( dialog->callID == callID_))
  {
    // The request cseq should be larger than our remotecseq.
    if(remoteCSeq <= remoteCSeq_ && type != ACK && type != CANCEL)
    {
      qDebug() << "PEER_ERROR:" << "Their Cseq was smaller than their previous cseq which is not permitted!";
      // TODO: if remote cseq in message is lower than remote cseq, send 500
      return false;
    }

    remoteCSeq_ = remoteCSeq;
    return true;
  }
  return false;
}

bool SIPDialogState::correctResponseDialog(std::shared_ptr<SIPDialogInfo> dialog, uint32_t messageCSeq)
{
  // For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).
  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if((dialog->fromTag == localTag_) && dialog->toTag == remoteTag_ || remoteTag_ == "" &&
     ( dialog->callID == callID_))
  {
    // The response cseq should be the same as our cseq
    if(messageCSeq != localCSeq_)
    {
      qDebug() << "PEER_ERROR:" << "The message CSeq was not the same as our previous request!";
      // TODO: if remote cseq in message is lower than remote cseq, send 500
      return false;
    }

    if(remoteTag_ == "")
    {
      qDebug() << "We don't yet have their remote Tag. Using the one in response.";
      remoteTag_ = dialog->toTag;
    }

    return true;
  }
  return false;
}

