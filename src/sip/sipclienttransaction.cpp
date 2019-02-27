#include "sipclienttransaction.h"

#include "siptransactionuser.h"

#include <QDebug>

// 2 seconds for a SIP reply
const unsigned int TIMEOUT = 2000;
// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPClientTransaction::SIPClientTransaction():
  ongoingTransactionType_(SIP_UNKNOWN_REQUEST),
  sessionID_(0),
  transactionUser_(nullptr)
{}

void SIPClientTransaction::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  Q_ASSERT(tu);
  transactionUser_ = tu;
  sessionID_ = sessionID;
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}


//processes incoming response
bool SIPClientTransaction::processResponse(SIPResponse &response)
{
  Q_ASSERT(sessionID_ != 0);
  Q_ASSERT(transactionUser_ != nullptr);

  qDebug() << "Client starts processing response";
  if(!sessionID_ || transactionUser_ == nullptr)
  {
    qWarning() << "WARNING: SIP Client Transaction not initialized.";
    return true;
  }

  int responseCode = response.type;

  // first process message type specific responses and the process general responses
  if(response.message->transactionRequest == INVITE)
  {
    if(responseCode == SIP_RINGING)
    {
      qDebug() << "Got provisional response. Restarting timer.";
      requestTimer_.start(INVITE_TIMEOUT);
      transactionUser_->callRinging(sessionID_);
      return true;
    }
    else if(responseCode == SIP_OK)
    {
      qDebug() << "Got response. Stopping timout.";
      requestTimer_.stop();
      transactionUser_->peerAccepted(sessionID_);
      requestSender(ACK);
      transactionUser_->callNegotiated(sessionID_);
      return true;
    }
    else if(responseCode == SIP_DECLINE)
    {
      requestTimer_.stop();
      qDebug() << "Got a Global Failure Response Code for INVITE";
      transactionUser_->peerRejected(sessionID_);
      return false;
    }
  }

  if(responseCode >= 300 && responseCode <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    qDebug() << "Got a redirection response Code. No processing implemented. Terminating dialog.";
  }
  else if(responseCode >= 400 && responseCode <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    qDebug() << "Got a Client Failure Response Code. No processing implemented. Terminating dialog.";

    // TODO: if the response is 481 or 408, terminate dialog
  }
  else if(responseCode >= 500 && responseCode <= 599)
  {
    qDebug() << "Got a Server Failure Response Code. No processing implemented. Terminating dialog.";
  }
  else if(responseCode >= 600 && responseCode <= 699
          && (response.message->transactionRequest != INVITE || responseCode != SIP_DECLINE))
  {
    qDebug() << "Got a Global Failure Response Code. No processing implemented. Terminating dialog.";
  }
  else if(responseCode == SIP_TRYING || responseCode == SIP_FORWARDED
          || responseCode == SIP_QUEUED)
  {
    requestTimer_.start(TIMEOUT);
    return true;
  }

  return false;
}


bool SIPClientTransaction::startCall()
{
  qDebug() << "Starting a call and sending an INVITE in session";
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    qWarning() << "WARNING: SIP Client Transaction not initialized";
    return false;
  }

  requestSender(INVITE);

  return true;
}

void SIPClientTransaction::endCall()
{
  qDebug() << "Ending the call with BYE";
  requestSender(BYE);
}

void SIPClientTransaction::cancelCall()
{
  requestSender(CANCEL);
}

void SIPClientTransaction::registerToServer()
{
  requestSender(REGISTER);
}

void SIPClientTransaction::getRequestMessageInfo(RequestType type,
                                                 std::shared_ptr<SIPMessageInfo>& outMessage)
{
  outMessage = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo);

  if(type == ACK)
  {
    outMessage->transactionRequest = INVITE;
  }
  else if(type == CANCEL)
  {
    outMessage->transactionRequest = ongoingTransactionType_;
  }
  else
  {
    outMessage->transactionRequest = type;
  }

  outMessage->dialog = nullptr;
  outMessage->maxForwards = 71;
  outMessage->version = "2.0";
  outMessage->cSeq = 0; // invalid, should be set in dialog
}

void SIPClientTransaction::requestSender(RequestType type)
{
  qDebug() << "Client starts sending a request:" << type;
  ongoingTransactionType_ = type;
  emit sendRequest(sessionID_, type);

  // INVITE has the same timeout as rest of them. Only after RINGING reply do we increase timeout
  if(type != CANCEL && type != ACK)
  {
    qDebug() << "Request timeout set to: " << TIMEOUT;
    requestTimer_.start(TIMEOUT);
  }
  else
  {
    requestTimer_.stop();
  }
}

void SIPClientTransaction::requestTimeOut()
{
  qDebug() << "No response. Request timed out";

  if(ongoingTransactionType_ == INVITE)
  {
    emit sendRequest(sessionID_, BYE);
    // TODO tell user we have failed
  }

  requestSender(CANCEL);
  requestTimer_.stop();
  transactionUser_->peerRejected(sessionID_);
  ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
}
