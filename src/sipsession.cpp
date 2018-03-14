#include "sipsession.h"
#include "common.h"

#include "siptransactionuser.h"

#include <QDebug>



const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;

SIPSession::SIPSession():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  sessionID_(0),
  cSeq_(1),
  registered_(false),
  ourSession_(false)
{}

void SIPSession::init(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  localTag_ = generateRandomString(TAGLENGTH);
  sessionID_ = sessionID;
}

void SIPSession::generateCallID(QString localAddress)
{
  if(ourSession_)
  {
    callID_ = generateRandomString(CALLIDLENGTH) + "@" + localAddress;
  }
}

std::shared_ptr<SIPMessageInfo> SIPSession::getRequestInfo(RequestType type)
{
  std::shared_ptr<SIPMessageInfo> message = generateMessage(type);
  message->session = std::shared_ptr<SIPSessionInfo> (new SIPSessionInfo{remoteTag_, localTag_, callID_});
  return message;
}

std::shared_ptr<SIPMessageInfo> SIPSession::getResponseInfo(RequestType ongoingTransaction)
{
  std::shared_ptr<SIPMessageInfo> message = generateMessage(ongoingTransaction);
  message->session = std::shared_ptr<SIPSessionInfo> (new SIPSessionInfo{localTag_, remoteTag_, callID_});
  return message;
}

bool SIPSession::correctRequest(std::shared_ptr<SIPSessionInfo> session)
{
  return (session->toTag == localTag_ || session->toTag == "") && (session->fromTag == remoteTag_ || remoteTag_ == "") &&
      ( session->callID == callID_ || callID_ == "");
}

bool SIPSession::correctResponse(std::shared_ptr<SIPSessionInfo> session)
{
  return session->fromTag == localTag_ && (session->toTag == remoteTag_ || remoteTag_ == "") &&
      ( session->callID == callID_ || callID_ == "");
}

std::shared_ptr<SIPMessageInfo> SIPSession::generateMessage(RequestType originalRequest)
{
  std::shared_ptr<SIPMessageInfo> mesg = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  mesg->session = NULL;
  mesg->routing = NULL;
  mesg->cSeq = cSeq_;
  ++cSeq_;

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





