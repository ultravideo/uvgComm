#include "sipdialog.h"
#include "common.h"

#include "siptransactionuser.h"

#include <QDateTime>
#include <QDebug>
#include <QSettings>

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;
const uint32_t BRANCHLENGTH = 32 - 7;

SIPDialog::SIPDialog():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  // cseq start value. For example 31-bits of 32-bit clock
  localCSeq_(QDateTime::currentSecsSinceEpoch()%2147483647),
  remoteCSeq_(0),
  secure_(false)
{}

void SIPDialog::init(SIP_URI remoteURI)
{
  initLocalURI();
  remoteUri_ = remoteURI;
}

void SIPDialog::createDialog(QString hostName)
{
  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(hostName != "")
  {
    callID_ += "@" + hostName;
  }

  qDebug() << "Local dialog created. CallID: " << callID_ << "Tag:" << localTag_ << "Cseq:" << localCSeq_;
}

void SIPDialog::initLocalURI()
{
  // init stuff from the settings
  QSettings settings;

  localUri_.realname = settings.value("local/Name").toString();
  localUri_.username = settings.value("local/Username").toString();

  if(localUri_.username.isEmpty())
  {
    localUri_.username = "anonymous";
  }

  // TODO: Get server URI from settings
  localUri_.host = "";
}

void SIPDialog::processFirstINVITE(std::shared_ptr<SIPDialogInfo> dialog, uint32_t cSeq, SIP_URI remoteUri)
{
  Q_ASSERT(callID_ != "");

  if(callID_ != "")
  {
    if(correctRequestDialog(dialog, INVITE, cSeq))
    {
      qDebug() << "Dialog: Got a Re-INVITE";
      return;
    }
    else
    {
      qDebug() << "ERROR: Got a request not belonging to this dialog";
    }
  }

  initLocalURI();

  remoteTag_ = dialog->fromTag;
  if(remoteTag_ == "")
  {
    qDebug() << "PEER_ERROR: They did not provide their tag in INVITE!";
  }

  remoteCSeq_ = cSeq;
  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = dialog->callID;

  qDebug() << "Received a dialog creating INVITE. Creating dialog."
           << "CallID: " << callID_ << "Tag:" << localTag_ << "Cseq:" << localCSeq_;
}

std::shared_ptr<SIPMessageInfo> SIPDialog::getRequestDialogInfo(RequestType type, QString localAddress)
{
  std::shared_ptr<SIPMessageInfo> message = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo);

  if(type != ACK && type != CANCEL)
  {
    ++localCSeq_;
  }

  message->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{remoteTag_, localTag_, callID_});
  message->cSeq = localCSeq_;
  message->version = "2.0";
  message->maxForwards = 71;
  message->dialog = NULL;
  message->from = localUri_;
  message->to = remoteUri_;
  message->contact = localUri_;
  message->senderReplyAddress.push_back(getLocalVia(localAddress));

  return message;
}

ViaInfo SIPDialog::getLocalVia(QString localAddress)
{
  return ViaInfo{TCP, "2.0", localAddress, QString("z9hG4bK" + generateRandomString(BRANCHLENGTH))};
}

bool SIPDialog::correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog, RequestType type, uint32_t remoteCSeq)
{
  Q_ASSERT(callID_ != "");
  if(callID_ == "")
  {
    qDebug() << "WARNING: The SIP dialog has not been initialized, but it is used";
    return false;
  }

  // For backwards compability, this should be prepared for missing To-tag (RFC3261).
  if((dialog->toTag == localTag_ || dialog->toTag == "") && (dialog->fromTag == remoteTag_) &&
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
