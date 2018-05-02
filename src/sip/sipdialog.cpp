#include "sipdialog.h"
#include "common.h"

#include "siptransactionuser.h"

#include <QDateTime>
#include <QDebug>

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;

SIPDialog::SIPDialog():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  // cseq start value. For example 31-bits of 32-bit clock
  localCSeq_(QDateTime::currentSecsSinceEpoch()%2147483647),
  remoteCSeq_(0),
  secure_(false)
{}

void SIPDialog::initDialog(QString serverName, SIP_URI localUri, SIP_URI remoteUri)
{
  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(serverName != "")
  {
    callID_ += "@" + serverName;
  }

  qDebug() << "Local dialog created. CallID: " << callID_ << "Tag:" << localTag_ << "Cseq:" << localCSeq_;

  // non-REGISTER outside the dialog, cseq is arbitary.
}

void SIPDialog::processINVITE(std::shared_ptr<SIPDialogInfo> dialog, uint32_t cSeq,
                              SIP_URI localUri, SIP_URI remoteUri)
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

  remoteTag_ = dialog->fromTag;
  if(remoteTag_ == "")
  {
    qDebug() << "PEER_ERROR: They did not provide their tag in INVITE!";
  }

  remoteCSeq_ = cSeq;
  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = dialog->callID;

  qDebug() << "Got a dialog creating INVITE. CallID: " << callID_ << "Tag:" << localTag_ << "Cseq:" << localCSeq_;
}

std::shared_ptr<SIPMessageInfo> SIPDialog::getRequestDialogInfo(RequestType type)
{
  if(type != ACK && type != CANCEL)
  {
    ++localCSeq_;
  }
  std::shared_ptr<SIPMessageInfo> message = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  message->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{remoteTag_, localTag_, callID_});
  message->cSeq = localCSeq_;

  return message;
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
