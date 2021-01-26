#include "sipclient.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPClient::SIPClient():
  ongoingTransactionType_(SIP_NO_REQUEST),
  sessionID_(0),
  transactionUser_(nullptr)
{
  requestTimer_.setSingleShot(true);
  connect(&requestTimer_, SIGNAL(timeout()), this, SLOT(requestTimeOut()));
}


SIPClient::~SIPClient()
{}


void SIPClient::setDialogStuff(SIPTransactionUser* tu, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  sessionID_ = sessionID;
  transactionUser_ = tu;
}


void SIPClient::setNonDialogStuff(SIP_URI& uri)
{
  remoteUri_ = uri;
}


bool SIPClient::transactionINVITE(QString callee)
{
  printNormal(this, "Starting a call and sending an INVITE in session");
  Q_ASSERT(sessionID_ != 0);
  if(!sessionID_)
  {
    printDebug(DEBUG_WARNING, this, "SIP Client Transaction not initialized.");
    return false;
  }

  if (startTransaction(SIP_INVITE))
  {
    transactionUser_->outgoingCall(sessionID_, callee);
  }

  return true;
}


void SIPClient::transactionBYE()
{
  startTransaction(SIP_BYE);
}


void SIPClient::transactionCANCEL()
{
  startTransaction(SIP_CANCEL);
}


void SIPClient::transactionReINVITE()// TODO: Remove
{
  startTransaction(SIP_INVITE);
}


void SIPClient::transactionREGISTER(uint32_t expires)
{
  expires_ = expires;
  if (startTransaction(SIP_REGISTER))
  {
    emit sendNondialogRequest(remoteUri_, SIP_REGISTER);
  }
}


bool SIPClient::processResponse(SIPResponse& response,
                                SIPDialogState &state)
{
  Q_UNUSED(state);

  printNormal(this, "Client starts processing response");

  int responseCode = response.type;

  if (!checkTransactionType(response.message->cSeq.method))
  {
    printPeerError(this, "Their response transaction type is not the same as our request!");
    if (transactionUser_ != nullptr)
    {
      transactionUser_->failure(sessionID_, response.text);
    }

    return false;
  }

  // provisional response, continuing
  if (responseCode >= 100 && responseCode <= 199)
  {
    printNormal(this, "Got a provisional response. Restarting timer.");
    if (response.message->cSeq.method == SIP_INVITE &&
        responseCode == SIP_RINGING)
    {
      startTimeoutTimer(INVITE_TIMEOUT);
    }
    else
    {
      startTimeoutTimer();
    }
  }

  // the transaction ends
  if (responseCode >= 200)
  {
    ongoingTransactionType_ = SIP_NO_REQUEST;
    stopTimeoutTimer();
  }

  if (responseCode >= 300 && responseCode <= 399)
  {
    // TODO: 8.1.3.4 Processing 3xx Responses in RFC 3261
    printWarning(this, "Got a Redirection Response.");
  }
  else if (responseCode >= 400 && responseCode <= 499)
  {
    // TODO: 8.1.3.5 Processing 4xx Responses in RFC 3261
    printWarning(this, "Got a Failure Response.");
  }
  else if (responseCode >= 500 && responseCode <= 599)
  {
    printWarning(this, "Got a Server Failure Response.");
  }
  else if (responseCode >= 600 && responseCode <= 699)
  {
    printWarning(this, "Got a Global Failure Response.");
  }

  if (responseCode >= 100 && responseCode <= 299)
  {

    // process anything that is not a failure and may cause a new request to be sent.
    // this must be done after the SIPClientTransaction processResponse, because that checks our
    // current active transaction and may modify it.
    if(response.message->cSeq.method == SIP_INVITE)
    {
      if(response.type == SIP_RINGING)
      {
        if (!state.getState())
        {
          transactionUser_->callRinging(sessionID_);
        }
      }
      else if(response.type == SIP_OK)
      {
        if (!state.getState())
        {
          transactionUser_->peerAccepted(sessionID_);
        }

        if (startTransaction(SIP_ACK))
        {
          state.setState(true);
          transactionUser_->callNegotiated(sessionID_);
        }
      }
    }
    else if(response.message->cSeq.method == SIP_BYE)
    {
      transactionUser_->endCall(sessionID_);
    }
    return true;
  }

  // delete dialog if response was not provisional or success.

  return false;
}


void SIPClient::getRequestMessageInfo(SIPRequestMethod type,
                                      std::shared_ptr<SIPMessageHeader>& outMessage)
{
  outMessage = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
  outMessage->cSeq.cSeq = 0; // invalid, should be set in dialog
  outMessage->cSeq.method = type;

  outMessage->maxForwards = std::shared_ptr<uint8_t> (new uint8_t{DEFAULT_MAX_FORWARDS});

  outMessage->contentType = MT_NONE;
  outMessage->contentLength = 0;

  // via is set later

  if (type == SIP_REGISTER)
  {
    outMessage->expires = std::shared_ptr<uint32_t> (new uint32_t{expires_});
  }

  // INVITE has the same timeout as rest of them. Only after RINGING reply do we increase timeout
  if(type != SIP_CANCEL && type != SIP_ACK)
  {
    startTimeoutTimer();
  }


}


bool SIPClient::startTransaction(SIPRequestMethod type)
{
  printDebug(DEBUG_NORMAL, this,
             "Client starts sending a request.", {"Type"}, {QString::number(type)});

  if (ongoingTransactionType_ != SIP_NO_REQUEST && type != SIP_CANCEL)
  {
    printProgramWarning(this, "Tried to send a request "
                              "while previous transaction has not finished");
    return false;
  }
  else if (ongoingTransactionType_ == SIP_NO_REQUEST && type == SIP_CANCEL)
  {
    printProgramWarning(this, "Tried to cancel a transaction that does not exist!");
    return false;
  }

  // we do not expect a response for these requests.
  if (type == SIP_CANCEL || type == SIP_ACK)
  {
    stopTimeoutTimer();
  }
  else
  {
    // cancel and ack don't start a transaction
    ongoingTransactionType_ = type;
  }

  emit sendDialogRequest(sessionID_, type);

  return true;
}


void SIPClient::processTimeout()
{
  if(getOngoingRequest() == SIP_INVITE)
  {
    emit sendDialogRequest(sessionID_, SIP_BYE);
    // TODO tell user we have failed
    startTransaction(SIP_CANCEL);
    transactionUser_->failure(sessionID_, "No response. Request timed out");
  }

  requestTimer_.stop();

  if (ongoingTransactionType_ == SIP_BYE)
  {
    byeTimeout();
  }

  ongoingTransactionType_ = SIP_NO_REQUEST;
}


void SIPClient::byeTimeout()
{
  transactionUser_->endCall(sessionID_);
  emit BYETimeout(sessionID_);
}


void SIPClient::requestTimeOut()
{
  printWarning(this, "No response. Request timed out.",
    {"Ongoing transaction"}, {QString::number(ongoingTransactionType_)});
  processTimeout();
}
