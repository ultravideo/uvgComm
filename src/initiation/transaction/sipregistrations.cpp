#include "sipregistrations.h"


#include "initiation/transaction/sipdialogstate.h"
#include "initiation/transaction/sipnondialogclient.h"

#include "common.h"


SIPRegistrations::SIPRegistrations()
{

}


void SIPRegistrations::init(SIPTransactionUser *callControl)
{
  printNormalDebug(this, DC_STARTUP, "Initiatin Registrations");
  transactionUser_ = callControl;
}


void SIPRegistrations::bindToServer(QString serverAddress, QHostAddress localAddress)
{
  qDebug() << "Binding to SIP server at:" << serverAddress;

  Q_ASSERT(transactionUser_);
  if(transactionUser_)
  {

    SIPRegistrationData data = {std::shared_ptr<SIPNonDialogClient> (new SIPNonDialogClient(transactionUser_)),
                                std::shared_ptr<SIPDialogState> (new SIPDialogState()), localAddress};

    SIP_URI serverUri = {"","",serverAddress, SIP};
    data.state->createServerDialog(serverUri, localAddress.toString());
    data.client->init();
    data.client->set_remoteURI(serverUri);
    registrations_[serverAddress] = data;

    QObject::connect(registrations_[serverAddress].client.get(),
                     &SIPNonDialogClient::sendNondialogRequest,
                     this, &SIPRegistrations::sendNonDialogRequest);

    registrations_[serverAddress].client->registerToServer();
  }
  else
  {
    printPErrorDebug(this, DC_REGISTRATION, "Not initialized with transaction user");
  }
}


bool SIPRegistrations::identifyRegistration(SIPResponse& response, QString &outAddress)
{
  outAddress = "";

  // check if this is a response from the server.
  for (auto i = registrations_.begin(); i != registrations_.end(); ++i)
  {
    if(i->second.state->correctResponseDialog(response.message->dialog,
                                              response.message->cSeq))
    {
      // TODO: we should check that every single detail is as specified in rfc.
      if(i->second.client->waitingResponse(response.message->transactionRequest))
      {
        qDebug() << "Found dialog matching the response";
        outAddress = i->first;
      }
      else
      {
        qDebug() << "PEER_ERROR: Found the server dialog, "
                    "but we have not sent a request to their response.";
        return false;
      }
    }
  }
  return outAddress != "";
}


void SIPRegistrations::processNonDialogResponse(SIPResponse& response)
{
  if (response.message->transactionRequest == SIP_REGISTER)
  {
    if (response.type == SIP_OK)
    {
      printNormalDebug(this, DC_REGISTRATION,
                       "Registration was succesful. TODO: update data structures");
    }
    else
    {
      printDebug(DEBUG_ERROR, this, DC_REGISTRATION, "REGISTER-request failed");
    }
  }
  else
  {
    printUnimplemented(this, "Processing of Non-REGISTER requests");
  }
}


void SIPRegistrations::sendNonDialogRequest(SIP_URI& uri, RequestType type)
{
  qDebug() << "Start sending of a non-dialog request. Type:" << type;
  SIPRequest request;
  request.type = type;

  if (type == SIP_REGISTER)
  {
    if (registrations_.find(uri.host) == registrations_.end())
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_REQUEST,
                 "Registration information should have been created "
                 "already before sending REGISTER message!");

      return;
    }

    registrations_[uri.host].client->getRequestMessageInfo(request.type, request.message);
    registrations_[uri.host].state->getRequestDialogInfo(request,
                                         registrations_[uri.host].localAddress.toString());

    QVariant content; // we dont have content in REGISTER
    emit transportProxyRequest(uri.host, request);
  }
  else if (type == SIP_OPTIONS) {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_REQUEST,
                     "Trying to send unimplemented non-dialog request OPTIONS!");
  }
  else {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_REQUEST,
                     "Trying to send a non-dialog request of type which is a dialog request!");
  }
}
