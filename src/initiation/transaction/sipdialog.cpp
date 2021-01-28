#include "sipdialog.h"

#include "common.h"

#include <QVariant>

SIPDialog::SIPDialog():
  state_(),
  client_(),
  server_()
{}

void SIPDialog::init(uint32_t sessionID, SIPTransactionUser* tu)
{
  client_.setDialogStuff(tu, sessionID);
  server_.init(tu, sessionID);

  QObject::connect(&client_, &SIPClient::sendDialogRequest,
                   this, &SIPDialog::generateRequest);

  QObject::connect(&client_, &SIPClient::BYETimeout,
                   this, &SIPDialog::dialogEnds);

  QObject::connect(&server_, &SIPServer::sendResponse,
                   this, &SIPDialog::generateResponse);
}

// 1 minute for the user to react
const unsigned int INVITE_TIMEOUT = 60000;

void SIPDialog::startCall(NameAddr &address, QString localAddress, bool registered)
{
  state_.createNewDialog(address);

  // in peer-to-peer calls we use the actual network address as local URI
  if (!registered)
  {
    state_.setLocalHost(localAddress);
  }

  // this start call will commence once the connection has been established
  if(!client_.transactionINVITE(address.realname, INVITE_TIMEOUT))
  {
    printWarning(this, "Could not start a call according to client.");
  }
}


void SIPDialog::createDialogFromINVITE(SIPRequest& invite,
                                       QString localAddress)
{
  QVariant content;
  state_.processIncomingRequest(invite, content);
  state_.setLocalHost(localAddress);
}


void SIPDialog::renegotiateCall()
{
  client_.transactionReINVITE();
}


void SIPDialog::acceptCall()
{
  server_.respondOK();
}


void SIPDialog::rejectCall()
{
  server_.respondDECLINE();
}


void SIPDialog::endCall()
{
  client_.transactionBYE();
}


void SIPDialog::cancelCall()
{
  client_.transactionCANCEL();
}


bool SIPDialog::isThisYours(SIPRequest& request)
{
  if (request.method == SIP_CANCEL)
  {
    return server_.isCANCELYours(request);
  }
  return state_.correctRequestDialog(request.message,
                                     request.method,
                                     request.message->cSeq.cSeq);
}


bool SIPDialog::isThisYours(SIPResponse& response)
{
  return
      state_.correctResponseDialog(response.message, response.message->cSeq.cSeq)
      && client_.waitingResponse(response.message->cSeq.method);
}


bool SIPDialog::processRequest(SIPRequest& request)
{
  return server_.processRequest(request);
}


bool SIPDialog::processResponse(SIPResponse& response)
{
  // TODO: prechecks that the response is ok, then modify program state.

  if (response.message == nullptr)
  {
    printProgramError(this, "SIPDialog got a message without header");
    return false;
  }

  QVariant content; // unused

  state_.processIncomingResponse(response, content);

  if(!client_.processResponse(response))
  {
    return false;
  }

  return true;
}


void SIPDialog::generateRequest(uint32_t sessionID, SIPRequest& request)
{
  printNormal(this, "Initiate sending of a dialog request");

  QVariant content; // unused
  state_.processOutgoingRequest(request, content);

  emit sendRequest(sessionID, request);
  printNormal(this, "Finished sending of a dialog request");
}


void SIPDialog::generateResponse(uint32_t sessionID, SIPResponse& response)
{
  // Get all the necessary information from different components.

  emit sendResponse(sessionID, response);
  printNormal(this, "Finished sending of a dialog response");
}
