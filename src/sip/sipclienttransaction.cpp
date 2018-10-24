#include "sipclienttransaction.h"

#include "siptransactionuser.h"

#include <QDebug>

// 2 seconds for a SIP reply
const unsigned int TIMEOUT = 2000;
// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPClientTransaction::SIPClientTransaction():
  ongoingTransactionType_(SIP_UNKNOWN_REQUEST),
  connected_(false),
  sessionID_(0),
  pendingRequest_(SIP_UNKNOWN_REQUEST),
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

  if(responseCode >= 100 && responseCode <= 199)
  {
    qDebug() << "Got provisional response. Restarting timer.";
    if(response.message->transactionRequest == INVITE)
    {
      requestTimer_.start(INVITE_TIMEOUT);
    }
    else
    {
      requestTimer_.start(TIMEOUT);
    }
  }
  else
  {
    qDebug() << "Got response. Stopping timout.";
    requestTimer_.stop();

    if(response.message->transactionRequest == INVITE)
    {
      transactionUser_->callAccepted(sessionID_);
    }

  }

  if(responseCode >= 300 && responseCode <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    qDebug() << "Got a redirection response Code. Not implemented!";
    return false;
  }

  if(responseCode >= 400 && responseCode <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    qDebug() << "Got a Client Failure Response Code. Not implemented!";

    // TODO: if the response is 481 or 408, terminate dialog
    return false;
  }

  if(responseCode >= 500 && responseCode <= 599)
  {
    qDebug() << "Got a Server Failure Response Code. Not implemented!";
    return false;
  }


  if(ongoingTransactionType_ == INVITE)
  {
    if(responseCode >= 600 && responseCode <= 699)
    {
      qDebug() << "Got a Global Failure Response Code for INVITE";
      transactionUser_->callRejected(sessionID_);
      return false;
    }
    else
    {
      switch (response.type) {
      case SIP_RINGING:
        transactionUser_->callRinging(sessionID_);
        break;
      case SIP_OK:
        // the sdp has been check by transaction layer before this.
        requestSender(ACK);
        transactionUser_->callNegotiated(sessionID_);
        break;
      default:
        break;
      }
    }
  }
  if(responseCode >= 200)
  {
    ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
  }

  return true;
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

void SIPClientTransaction::registerToServer()
{
  requestSender(REGISTER);
}

void SIPClientTransaction::connectionReady(bool ready)
{
  connected_ = ready;
  if(pendingRequest_ != SIP_UNKNOWN_REQUEST)
  {
    requestSender(pendingRequest_);
    pendingRequest_ = SIP_UNKNOWN_REQUEST;
  }
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
  if(!connected_)
  {
    qDebug() << "Added a pending request:" << type;
    pendingRequest_ = type;
  }
  else
  {
    qDebug() << "Client starts sending a request:" << type;
    ongoingTransactionType_ = type;
    emit sendRequest(sessionID_, type);

    if(type != INVITE && type != CANCEL && type != ACK)
    {
      qDebug() << "Request timeout set to: " << TIMEOUT;
      requestTimer_.start(TIMEOUT);
    }
    else if(type == INVITE) // INVITE has longer timeout while waiting for peer
    {
      qDebug() << "INVITE timeout set to: " << INVITE_TIMEOUT;
      requestTimer_.start(INVITE_TIMEOUT);
    }
    else
    {
      requestTimer_.stop();
    }
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
  transactionUser_->callRejected(sessionID_);
  ongoingTransactionType_ = SIP_UNKNOWN_REQUEST;
}
