#include "sipdialogclient.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

#include <QDebug>



SIPDialogClient::SIPDialogClient():
  sessionID_(0),
  transactionUser_(nullptr)
{}

void SIPDialogClient::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  sessionID_ = sessionID;

  transactionUser_ = tu;
}


void SIPDialogClient::getRequestMessageInfo(RequestType type,
                                            std::shared_ptr<SIPMessageInfo> &outMessage)
{
  SIPClient::getRequestMessageInfo(type, outMessage);

  if (type == SIP_INVITE || type == SIP_ACK)
  {
    if (!outMessage->vias.empty())
    {
      outMessage->vias.back().rport = true;
    }
  }
}


//processes incoming response
bool SIPDialogClient::processResponse(SIPResponse& response,
                                      SIPDialogState &state)
{
  printNormal(this, "Client starts processing response");
  Q_ASSERT(sessionID_ != 0);

  if(!sessionID_)
  {
    printProgramError(this, "SIP Client Transaction not initialized.");
    return true;
  }

  if (!checkTransactionType(response.message->transactionRequest))
  {
    printPeerError(this, "Their response transaction type is not the same as our request!");
    return false;
  }

  // check if this is failure that requires shutdown of session
  if (!SIPClient::processResponse(response, state))
  {
    printWarning(this, "Got a failure response!");
    transactionUser_->failure(sessionID_, response.text);
    return false;
  }

  // process anything that is not a failure and may cause a new request to be sent.
  // this must be done after the SIPClientTransaction processResponse, because that checks our
  // current active transaction and may modify it.
  if(response.message->transactionRequest == SIP_INVITE)
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
  else if(response.message->transactionRequest == SIP_BYE)
  {
    transactionUser_->endCall(sessionID_);
  }

  return true;
}


bool SIPDialogClient::startCall(QString callee)
{
  qDebug() << "SIP, Dialog client: Starting a call and sending an INVITE in session";
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


void SIPDialogClient::requestEnd()
{
  qDebug() << "Ending the call with BYE";
  startTransaction(SIP_BYE);
}


void SIPDialogClient::requestCancel()
{
  startTransaction(SIP_CANCEL);
}


void SIPDialogClient::requestRenegotiation()
{
  startTransaction(SIP_INVITE);
}


void SIPDialogClient::processTimeout()
{
  if(getOngoingRequest() == SIP_INVITE)
  {
    emit sendDialogRequest(sessionID_, SIP_BYE);
    // TODO tell user we have failed
    startTransaction(SIP_CANCEL);
    transactionUser_->failure(sessionID_, "No response. Request timed out");
  }

  SIPClient::processTimeout();

  // destroys the whole dialog
  if (getOngoingRequest() == SIP_BYE)
  {
    emit BYETimeout(sessionID_);
  }
}


bool SIPDialogClient::startTransaction(RequestType type)
{
  if (SIPClient::startTransaction(type))
  {
    emit sendDialogRequest(sessionID_, type);
    return true;
  }
  return false;
}
