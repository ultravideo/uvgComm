#include "sipregistration.h"


#include "initiation/transaction/sipdialogstate.h"
#include "initiation/transaction/sipclient.h"

#include "common.h"
#include "serverstatusview.h"
#include "global.h"


#include <QDebug>

const int REGISTER_SEND_PERIOD = (REGISTER_INTERVAL - 5)*1000;


SIPRegistration::SIPRegistration():
  retryTimer_(nullptr)
{}


SIPRegistration::~SIPRegistration()
{}


void SIPRegistration::init(ServerStatusView *statusView)
{
  printNormal(this, "Initiating Registration");
  statusView_ = statusView;

  QObject::connect(&retryTimer_, &QTimer::timeout,
                   this, &SIPRegistration::refreshRegistration);

  retryTimer_.setInterval(REGISTER_SEND_PERIOD); // have 5 seconds extra to reach registrar
  retryTimer_.setSingleShot(false);
}


void SIPRegistration::uninit()
{
  if (status_ == REG_ACTIVE)
  {
    status_ = DEREGISTERING;
    client_.setNextTransactionExpires(0);
    client_.sendRequest(SIP_REGISTER);
  }

  printNormal(this, "Finished uniniating registration");

  // we don't wait for the OK reply so we can quit faster.
  return;
}


void SIPRegistration::bindToServer(QString serverAddress, QString localAddress,
                                    uint16_t port)
{
  qDebug() << "Binding to SIP server at:" << serverAddress;

  status_ = INACTIVE;
  contactAddress_ = localAddress;
  contactPort_ = port;

  serverAddress_ = serverAddress;

  SIP_URI serverUri = {DEFAULT_SIP_TYPE, {"", ""}, {serverAddress, 0}, {}, {}};
  state_.setLocalHost(localAddress);
  state_.createServerConnection(serverUri);

  QObject::connect(&client_, &SIPClient::outgoingRequest,
                   this, &SIPRegistration::sendNonDialogRequest);

  statusView_->updateServerStatus("Request sent. Waiting response...");
  status_ = FIRST_REGISTRATION;
  client_.setNextTransactionExpires(REGISTER_INTERVAL);
  client_.sendRequest(SIP_REGISTER);
}


bool SIPRegistration::identifyRegistration(SIPResponse& response)
{
  // check if this is a response from the server.
  if(state_.correctResponseDialog(response.message,
                                 response.message->cSeq.cSeq, false))
  {
    // TODO: we should check that every single detail is as specified in rfc.
    if(client_.waitingResponse(response.message->cSeq.method))
    {
      printNormal(this, "Found registration matching the response");
      return true;
    }
    else
    {
      qDebug() << "PEER_ERROR: Found the server dialog, "
                  "but we have not sent a request to their response.";
      return false;
    }
  }
  return false;
}


void SIPRegistration::processNonDialogResponse(SIPResponse& response)
{
  // REGISTER response must not create route. In other words ignore all record-routes

  if (response.message->cSeq.method == SIP_REGISTER)
  {
    bool foundRegistration = false;

    if (serverAddress_ == response.message->to.address.uri.hostport.host)
    {
      QVariant content; // unused
      client_.processIncomingResponse(response, content);

      /*
      if (!i.second->client.shouldBeKeptAlive())
      {
        printWarning(this, "Got a failure response to our REGISTER");
        i.second->status = INACTIVE;
        return;
      }
      */

      if (response.type == SIP_OK)
      {
        foundRegistration = true;

        if (status_ != RE_REGISTRATION &&
            response.message->vias.at(0).receivedAddress != "" &&
            response.message->vias.at(0).rportValue != 0 &&
            (contactAddress_ != response.message->vias.at(0).receivedAddress ||
             contactPort_ != response.message->vias.at(0).rportValue))
        {
          printNormal(this, "Detected that we are behind NAT!");

          // we want to remove the previous registration so it doesn't cause problems
          if (status_ == FIRST_REGISTRATION)
          {
            printNormal(this, "Resetting previous registration");
            status_ = DEREGISTERING;
            client_.setNextTransactionExpires(0);
            client_.sendRequest(SIP_REGISTER);
            return;
          }
          else if (status_ == DEREGISTERING)// the actual NAT registration
          {
            status_ = RE_REGISTRATION;
            printNormal(this, "Sending the final NAT REGISTER");
            contactAddress_ = response.message->contact.first().address.uri.hostport.host;
            contactPort_ = response.message->contact.first().address.uri.hostport.port;
            // makes sure we don't end up in infinite loop if the address doesn't match

            statusView_->updateServerStatus("Behind NAT, updating address...");

             // re-REGISTER with NAT address and port
            client_.setNextTransactionExpires(REGISTER_INTERVAL);
            client_.sendRequest(SIP_REGISTER);
            return;
          }
          else
          {
            printError(this, "The Registration response does not match internal state");
          }
        }
        else
        {
          statusView_->updateServerStatus("Registered");
        }

        status_ = REG_ACTIVE;

        if (!retryTimer_.isActive())
        {
          retryTimer_.start(REGISTER_SEND_PERIOD);
        }

        printNormal(this, "Registration was successful.");
      }
      else
      {
        printDebug(DEBUG_ERROR, this, "REGISTER-request failed");
        statusView_->updateServerStatus(response.text);
      }
    }

    if (!foundRegistration)
    {
      qDebug() << "PEER ERROR: Got a resonse to REGISTRATION we didn't send";
    }
  }
  else
  {
    printUnimplemented(this, "Processing of Non-REGISTER requests");
  }
}


void SIPRegistration::refreshRegistration()
{
  // no need to continue refreshing if we have no active registrations
  if (!haveWeRegistered())
  {
    printWarning(this, "Not refreshing our registrations, because we have none!");
    retryTimer_.stop();
    return;
  }

  if (status_ == REG_ACTIVE)
  {
    statusView_->updateServerStatus("Second request sent. Waiting response...");
    client_.setNextTransactionExpires(REGISTER_INTERVAL);
    client_.sendRequest(SIP_REGISTER);
  }
}


bool SIPRegistration::haveWeRegistered()
{
  return status_ == REG_ACTIVE;
}


void SIPRegistration::sendNonDialogRequest(SIPRequest& request, QVariant& content)
{
  printNormal(this, "Send non-dialog request");

  if (request.method == SIP_REGISTER)
  {
    state_.processOutgoingRequest(request, content);
  }

  emit transportProxyRequest(serverAddress_, request);
}
