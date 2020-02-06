#include "sipdialogclient.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

#include <QDebug>



SIPDialogClient::SIPDialogClient(SIPTransactionUser* tu):SIPClientTransaction(tu),
  sessionID_(0)
{}


void SIPDialogClient::setSessionID(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  sessionID_ = sessionID;
}


void SIPDialogClient::getRequestMessageInfo(RequestType type,
                                            std::shared_ptr<SIPMessageInfo> &outMessage)
{
  SIPClientTransaction::getRequestMessageInfo(type, outMessage);

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
                                      std::shared_ptr<SIPDialogState> state)
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

  // first process message type specific responses and the process general responses
  if(response.message->transactionRequest == SIP_INVITE)
  {
    if(response.type == SIP_RINGING)
    {
      if (!state->getState())
      {
        getTransactionUser()->callRinging(sessionID_);
      }
    }
    else if(response.type == SIP_OK)
    {
      if (!state->getState())
      {
        getTransactionUser()->peerAccepted(sessionID_);
      }
      startTransaction(SIP_ACK);
      state->setState(true);
      getTransactionUser()->callNegotiated(sessionID_);
    }
    else if(response.type == SIP_DECLINE)
    {
      qDebug() << "Got a Global Failure Response Code for INVITE";
      getTransactionUser()->peerRejected(sessionID_);
    }
  }

  return SIPClientTransaction::processResponse(response, state);
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

  startTransaction(SIP_INVITE);

  getTransactionUser()->outgoingCall(sessionID_, callee);

  return true;
}


void SIPDialogClient::endCall()
{
  qDebug() << "Ending the call with BYE";
  startTransaction(SIP_BYE);
}


void SIPDialogClient::cancelCall()
{
  startTransaction(SIP_CANCEL);
}


void SIPDialogClient::renegotiateCall()
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
    getTransactionUser()->peerRejected(sessionID_);
  }

  SIPClientTransaction::processTimeout();
}


void SIPDialogClient::startTransaction(RequestType type)
{
  emit sendDialogRequest(sessionID_, type);
  SIPClientTransaction::startTransaction(type);
}
