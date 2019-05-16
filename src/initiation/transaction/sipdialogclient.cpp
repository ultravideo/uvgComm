#include "sipdialogclient.h"

#include "initiation/siptransactionuser.h"

#include "common.h"

#include <QDebug>

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

SIPDialogClient::SIPDialogClient(SIPTransactionUser* tu):SIPClientTransaction(tu),
  sessionID_(0)
{}

void SIPDialogClient::setSessionID(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  sessionID_ = sessionID;
}

//processes incoming response
bool SIPDialogClient::processResponse(SIPResponse &response)
{
  Q_ASSERT(sessionID_ != 0);

  qDebug() << "Client starts processing response";
  if(!sessionID_)
  {
    printDebug(DEBUG_WARNING, this, "SIP Client Transaction not initialized.");
    return true;
  }

  int responseCode = response.type;

  // first process message type specific responses and the process general responses
  if(response.message->transactionRequest == SIP_INVITE)
  {
    if(responseCode == SIP_RINGING)
    {
      qDebug() << "Got provisional response. Restarting timer.";
      startTimeoutTimer(INVITE_TIMEOUT);
      getTransactionUser()->callRinging(sessionID_);
      return true;
    }
    else if(responseCode == SIP_OK)
    {
      qDebug() << "Got response. Stopping timout.";
      stopTimeoutTimer();
      getTransactionUser()->peerAccepted(sessionID_);
      sendRequest(SIP_ACK);
      getTransactionUser()->callNegotiated(sessionID_);
      return true;
    }
    else if(responseCode == SIP_DECLINE)
    {
      stopTimeoutTimer();
      qDebug() << "Got a Global Failure Response Code for INVITE";
      getTransactionUser()->peerRejected(sessionID_);
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
          && (response.message->transactionRequest != SIP_INVITE || responseCode != SIP_DECLINE))
  {
    qDebug() << "Got a Global Failure Response Code. No processing implemented. Terminating dialog.";
  }
  else if(responseCode == SIP_TRYING || responseCode == SIP_FORWARDED
          || responseCode == SIP_QUEUED)
  {
    startTimeoutTimer();
    return true;
  }

  return false;
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

  sendRequest(SIP_INVITE);

  getTransactionUser()->outgoingCall(sessionID_, callee);

  return true;
}

void SIPDialogClient::endCall()
{
  qDebug() << "Ending the call with BYE";
  sendRequest(SIP_BYE);
}

void SIPDialogClient::cancelCall()
{
  sendRequest(SIP_CANCEL);
}


void SIPDialogClient::processTimeout()
{
  if(getOngoingRequest() == SIP_INVITE)
  {
    emit sendDialogRequest(sessionID_, SIP_BYE);
    // TODO tell user we have failed
    sendRequest(SIP_CANCEL);
    getTransactionUser()->peerRejected(sessionID_);
  }

  SIPClientTransaction::processTimeout();
}

void SIPDialogClient::sendRequest(RequestType type)
{
  emit sendDialogRequest(sessionID_, type);
  SIPClientTransaction::sendRequest(type);
}
