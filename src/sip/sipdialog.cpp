#include "sipdialog.h"
#include "common.h"

#include "siptransactionuser.h"

#include <QDateTime>
#include <QDebug>
#include <QSettings>


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

  // assume we are using a peer-to-peer connecion and don't know our host address until now
  if(localUri_.host == "")
  {
    localUri_.host = hostName;
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

void SIPDialog::getRequestDialogInfo(RequestType type, QString localAddress,
                                     std::shared_ptr<SIPMessageInfo>& outMessage)
{
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
