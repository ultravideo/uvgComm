#include "sipdialog.h"

#include "common.h"

SIPDialog::SIPDialog():
  state_(),
  client_(),
  server_()
{}

void SIPDialog::init(uint32_t sessionID, SIPTransactionUser* tu)
{
  client_.init(tu, sessionID);
  server_.init(tu, sessionID);

  QObject::connect(&client_, &SIPDialogClient::sendDialogRequest,
                   this, &SIPDialog::generateRequest);

  QObject::connect(&client_, &SIPDialogClient::BYETimeout,
                   this, &SIPDialog::dialogEnds);

  QObject::connect(&server_, &SIPServer::sendResponse,
                   this, &SIPDialog::generateResponse);
}


void SIPDialog::startCall(SIP_URI &address, QString localAddress, bool registered)
{
  state_.createNewDialog(SIP_URI{TRANSPORTTYPE,
                                 {address.user.username, ""},
                                 address.realname,
                                 {address.hostport.host,  0}, {}},
                                 localAddress, registered);

  // this start call will commence once the connection has been established
  if(!client_.startCall(address.realname))
  {
    printWarning(this, "Could not start a call according to client.");
  }
}


void SIPDialog::createDialogFromINVITE(std::shared_ptr<SIPMessageInfo> &invite,
                                       QString localAddress)
{
  state_.createDialogFromINVITE(invite, localAddress);
}


void SIPDialog::renegotiateCall()
{
  client_.requestRenegotiation();
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
  client_.requestEnd();
}


void SIPDialog::cancelCall()
{
  client_.requestCancel();
}


bool SIPDialog::isThisYours(SIPRequest& request)
{
  if (request.type == SIP_CANCEL)
  {
    return server_.isCancelYours(request.message);
  }
  return state_.correctRequestDialog(request.message->dialog,
                                     request.type,
                                     request.message->cSeq);
}


bool SIPDialog::isThisYours(SIPResponse& response)
{
  return
      state_.correctResponseDialog(response.message->dialog, response.message->cSeq)
      && client_.waitingResponse(response.message->transactionRequest);
}


bool SIPDialog::processRequest(SIPRequest& request)
{
  return server_.processRequest(request, state_);
}


bool SIPDialog::processResponse(SIPResponse& response)
{
  // TODO: prechecks that the response is ok, then modify program state.

  if (response.type == SIP_OK && response.message->transactionRequest == SIP_INVITE)
  {
    state_.setRequestUri(response.message->contact);
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


void SIPDialog::generateRequest(uint32_t sessionID, RequestType type)
{
  printNormal(this, "Iniate sending of a dialog request");

  // Get all the necessary information from different components.
  SIPRequest request;
  if (type != SIP_CANCEL)
  {
    request.type = type;

    // Get message info
    client_.getRequestMessageInfo(type, request.message);
    client_.startTimer(type);

    state_.getRequestDialogInfo(request);

    Q_ASSERT(request.message != nullptr);
    Q_ASSERT(request.message->dialog != nullptr);

    client_.recordRequest(request);
  }
  else
  {
    request = client_.getRecordedRequest();

    request.type = SIP_CANCEL;
    request.message->transactionRequest = SIP_CANCEL;
  }

  emit sendRequest(sessionID, request);
  printNormal(this, "Finished sending of a dialog request");
}


void SIPDialog::generateResponse(uint32_t sessionID, ResponseType type)
{
  printNormal(this, "Iniate sending of a dialog response");

  // Get all the necessary information from different components.
  SIPResponse response;
  response.type = type;
  server_.getResponseMessage(response.message, type);

  emit sendResponse(sessionID, response);
  printNormal(this, "Finished sending of a dialog response");
}
