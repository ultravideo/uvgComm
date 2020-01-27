#include "sipregistrations.h"


#include "initiation/transaction/sipdialogstate.h"
#include "initiation/transaction/sipnondialogclient.h"

#include "common.h"

#include <QDebug>


SIPRegistrations::SIPRegistrations()
{

}


void SIPRegistrations::init(SIPTransactionUser *callControl)
{
  printNormalDebug(this, DC_STARTUP, "Initiatin Registrations");
  transactionUser_ = callControl;
}


void SIPRegistrations::bindToServer(QString serverAddress, QString localAddress,
                                    uint16_t port)
{
  qDebug() << "Binding to SIP server at:" << serverAddress;

  Q_ASSERT(transactionUser_);
  if(transactionUser_)
  {
    SIPRegistrationData data = {std::shared_ptr<SIPNonDialogClient> (new SIPNonDialogClient(transactionUser_)),
                                std::shared_ptr<SIPDialogState> (new SIPDialogState()),
                                localAddress, port, false, false};

    SIP_URI serverUri = {TRANSPORTTYPE, "", "", serverAddress, 0, {}};
    data.state->createServerConnection(serverUri);
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
  // REGISTER response must not create route. In other words ignore all record-routes

  if (response.message->transactionRequest == SIP_REGISTER)
  {
    if (response.type == SIP_OK)
    {
      bool foundRegistration = false;

      for (auto& i : registrations_)
      {
        if (i.first == response.message->to.host)
        {
          if (i.second.client->processResponse(response, i.second.state))
          {
            qDebug() << "SHOULD DELETE REGISTRATION!?!?!?!";
          }

          i.second.active = true;

          foundRegistration = true;

          if (i.second.localAddress != response.message->vias.at(0).receivedAddress ||
              i.second.localPort != response.message->vias.at(0).rportValue)
          {

            qDebug() << "Detected that we are behind NAT! Sending a second REGISTER";
            i.second.state->setOurContactAddress(response.message->vias.at(0).receivedAddress,
                                                 response.message->vias.at(0).rportValue);

            i.second.behindNAT = true;
            i.second.client->registerToServer(); // re-REGISTER with NAT address and port
          }

          printNormalDebug(this, DC_REGISTRATION,
                           "Registration was succesful.");
        }
      }

      if (!foundRegistration)
      {
        qDebug() << "PEER ERROR: Got a resonse to REGISTRATION we didn't send";
      }

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


bool SIPRegistrations::haveWeRegistered()
{
  bool registered = false;

  for (auto& i : registrations_)
  {
    if (i.second.active)
    {
      registered = true;
    }
  }

  return registered;
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
    registrations_[uri.host].state->getRequestDialogInfo(request);

    request.message->setContactAddress = !registrations_[uri.host].behindNAT;

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
