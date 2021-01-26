#include "sipdialog.h"

#include "common.h"

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


void SIPDialog::startCall(NameAddr &address, QString localAddress, bool registered)
{
  state_.createNewDialog(address, localAddress, registered);

  // this start call will commence once the connection has been established
  if(!client_.transactionINVITE(address.realname))
  {
    printWarning(this, "Could not start a call according to client.");
  }
}


void SIPDialog::createDialogFromINVITE(std::shared_ptr<SIPMessageHeader> &invite,
                                       QString localAddress)
{
  state_.createDialogFromINVITE(invite, localAddress);
}


void SIPDialog::renegotiateCall()
{
  client_.transactionReINVITE();
}


void SIPDialog::acceptCall()
{
  server_.responseAccept();
}


void SIPDialog::rejectCall()
{
  server_.respondReject();
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
    return server_.isCancelYours(request.message);
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
  return server_.processRequest(request, state_);
}


bool SIPDialog::processResponse(SIPResponse& response)
{
  // TODO: prechecks that the response is ok, then modify program state.

  if (response.message == nullptr)
  {
    printProgramError(this, "SIPDialog got a message without header");
    return false;
  }

  if (response.type == SIP_OK &&
      response.message->cSeq.method == SIP_INVITE &&
      !response.message->contact.empty())
  {
    state_.setRequestUri(response.message->contact.first().address.uri);
  }

  if(!client_.processResponse(response, state_))
  {
    return false;
  }
  else if (!response.message->recordRoutes.empty())
  {
    state_.setRoute(response.message->recordRoutes);
  }

  return true;
}


void SIPDialog::generateRequest(uint32_t sessionID, SIPRequestMethod type)
{
  printNormal(this, "Iniate sending of a dialog request");

  // Get all the necessary information from different components.
  SIPRequest request;

  if (type != SIP_CANCEL)
  {
    request.sipVersion = SIP_VERSION;
    // Get message info
    client_.getRequestMessageInfo(type, request.message);
    client_.startTimer(type);

    state_.getRequestDialogInfo(request);

    Q_ASSERT(request.message != nullptr);

    client_.recordRequest(request);
  }
  else
  {
    request = client_.getRecordedRequest();
    request.message->cSeq.method = SIP_CANCEL;
  }
  request.method = type;

  emit sendRequest(sessionID, request);
  printNormal(this, "Finished sending of a dialog request");
}


void SIPDialog::generateResponse(uint32_t sessionID, SIPResponseStatus type)
{
  printNormal(this, "Iniate sending of a dialog response");

  // Get all the necessary information from different components.
  SIPResponse response;
  response.type = type;
  response.sipVersion = SIP_VERSION;
  server_.getResponseMessage(response.message, type);

  emit sendResponse(sessionID, response);
  printNormal(this, "Finished sending of a dialog response");
}
