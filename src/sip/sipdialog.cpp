#include "sipdialog.h"
#include "common.h"

#include "siptransactionuser.h"

#include <QDebug>

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;

SIPDialog::SIPDialog():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  sessionID_(0),
  localCSeq_(1),
  remoteCSeq_(0),
  registered_(false),
  ourDialog_(false)
{}

void SIPDialog::init(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  localTag_ = generateRandomString(TAGLENGTH);
  sessionID_ = sessionID;
  // TODO: choose a better cseq start value. For example 31-bits of 32-bit clock
  // non-REGISTER outside the dialog, cseq is arbitary.
}

void SIPDialog::generateCallID(QString localAddress)
{
  if(ourDialog_)
  {
    callID_ = generateRandomString(CALLIDLENGTH) + "@" + localAddress;
  }
}

std::shared_ptr<SIPMessageInfo> SIPDialog::getRequestInfo(RequestType type)
{
  std::shared_ptr<SIPMessageInfo> message = generateMessage(type);
  message->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{remoteTag_, localTag_, callID_});
  return message;
}

std::shared_ptr<SIPMessageInfo> SIPDialog::getResponseInfo(RequestType ongoingTransaction)
{
  std::shared_ptr<SIPMessageInfo> message = generateMessage(ongoingTransaction);
  message->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{localTag_, remoteTag_, callID_});
  return message;
}

bool SIPDialog::processRequest(std::shared_ptr<SIPDialogInfo> dialog)
{
  // RFC3261_TODO: For backwards compability, this should be prepared for missing To-tag.

  if(callID_ == "")
  {
    callID_ = dialog->callID;
  }

  // TODO: if remote cseq in message is lower than remote cseq, send 500
  // The request cseq should be larger than our remotecseq.

  return (dialog->toTag == localTag_ || dialog->toTag == "") && (dialog->fromTag == remoteTag_ || remoteTag_ == "") &&
      ( dialog->callID == callID_);
}

bool SIPDialog::processResponse(std::shared_ptr<SIPDialogInfo> dialog)
{
  // RFC3261_TODO: For backwards compability, this should be prepared for missing To-tag.

  return dialog->fromTag == localTag_ && (dialog->toTag == remoteTag_ || remoteTag_ == "") &&
      ( dialog->callID == callID_ || callID_ == "");
}

std::shared_ptr<SIPMessageInfo> SIPDialog::generateMessage(RequestType originalRequest)
{
  std::shared_ptr<SIPMessageInfo> mesg = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  mesg->dialog = NULL;
  mesg->routing = NULL;
  mesg->cSeq = localCSeq_; // TODO: ACK and CANCEL don't increase cSeq
  if(originalRequest != ACK && originalRequest != CANCEL)
  {
    ++localCSeq_;
  }

  mesg->version = "2.0";
  if(originalRequest == ACK)
  {
    mesg->transactionRequest = INVITE;
  }
  else
  {
    mesg->transactionRequest = originalRequest;
  }
  return mesg;
}
