#include "sipregistration.h"

#include "serverstatusview.h"

#include "common.h"
#include "global.h"
#include "logger.h"

#include <QVariant>

const int REGISTER_SEND_PERIOD = (REGISTER_INTERVAL - 5)*1000;


SIPRegistration::SIPRegistration():
  retryTimer_(nullptr),
  attemptedAuth_(false)
{}


SIPRegistration::~SIPRegistration()
{}


void SIPRegistration::init(ServerStatusView *statusView)
{
  Logger::getLogger()->printNormal(this, "Initiating Registration");
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
    sendREGISTERRequest(0, DEREGISTERING);
  }

  Logger::getLogger()->printNormal(this, "Finished uniniating registration");
  return;
}


void SIPRegistration::bindToServer(NameAddr& addressRecord, QString localAddress,
                                    uint16_t port)
{
  Logger::getLogger()->printNormal(this, "Binding to server", {"Server"},
                                   {addressRecord.uri.hostport.host});

  status_ = INACTIVE;
  contactAddress_ = localAddress;
  contactPort_ = port;

  serverAddress_ = addressRecord.uri.hostport.host;
  statusView_->updateServerStatus("Request sent. Waiting response...");

  sendREGISTERRequest(REGISTER_INTERVAL, FIRST_REGISTRATION);
}


void SIPRegistration::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  Q_UNUSED(content);
  // REGISTER response must not create route. In other words ignore all record-routes

  if (response.message->cSeq.method == SIP_REGISTER)
  {
    if (serverAddress_ == response.message->to.address.uri.hostport.host)
    {
      if (response.type == SIP_OK)
      {
        attemptedAuth_ = false; // reset our attempts so if the digest expires,
                                // we auth again

        if (status_ != RE_REGISTRATION &&
            response.message->vias.at(0).receivedAddress != "" &&
            response.message->vias.at(0).rportValue != 0 &&
            (contactAddress_ != response.message->vias.at(0).receivedAddress ||
             contactPort_ != response.message->vias.at(0).rportValue))
        {
          Logger::getLogger()->printNormal(this, "Detected that we are behind NAT!");

          // we want to remove the previous registration so it doesn't cause problems
          if (status_ == FIRST_REGISTRATION)
          {
            Logger::getLogger()->printNormal(this, "Resetting previous registration");
            sendREGISTERRequest(0, DEREGISTERING);
            return;
          }
          else if (status_ == DEREGISTERING)// the actual NAT registration
          {
            if (!response.message->contact.empty())
            {
              Logger::getLogger()->printNormal(this, "Sending the final NAT REGISTER");
              contactAddress_ = response.message->contact.first().address.uri.hostport.host;
              contactPort_ = response.message->contact.first().address.uri.hostport.port;
              // makes sure we don't end up in infinite loop if the address doesn't match

              statusView_->updateServerStatus("Behind NAT, updating address...");

               // re-REGISTER with NAT address and port
              sendREGISTERRequest(REGISTER_INTERVAL, RE_REGISTRATION);
            }
            else
            {
              Logger::getLogger()->printError(this, "Failed to get contacts in REGISTER response");
            }
            return;
          }
          else
          {
            Logger::getLogger()->printError(this, "The Registration response does not match internal state");
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

        Logger::getLogger()->printNormal(this, "Registration was successful.");
      }
      else if (response.type == SIP_UNAUTHORIZED)
      {
        if (!attemptedAuth_)
        {
          attemptedAuth_ = true;
          sendREGISTERRequest(REGISTER_INTERVAL, status_);
        }
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_ERROR, this, "REGISTER-request failed");
        statusView_->updateServerStatus(response.text);
      }
    }
    else
    {
      Logger::getLogger()->printPeerError(this, "Got a resonse to REGISTRATION we didn't send");
    }
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Processing of Non-REGISTER requests");
  }
}


void SIPRegistration::refreshRegistration()
{
  // no need to continue refreshing if we have no active registrations
  if (!haveWeRegistered())
  {
    Logger::getLogger()->printWarning(this, "Not refreshing our registrations, because we have none!");
    retryTimer_.stop();
    return;
  }

  if (status_ == REG_ACTIVE)
  {
    statusView_->updateServerStatus("Second request sent. Waiting response...");
    sendREGISTERRequest(REGISTER_INTERVAL, REG_ACTIVE);
  }
}


bool SIPRegistration::haveWeRegistered()
{
  return status_ == REG_ACTIVE;
}


void SIPRegistration::sendREGISTERRequest(uint32_t expires, RegistrationStatus newStatus)
{
  SIPRequest request;
  request.method = SIP_REGISTER;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
  request.message->expires = std::shared_ptr<uint32_t> (new uint32_t{expires});

  QVariant content;

  status_ = newStatus;
  emit outgoingRequest(request, content);
}
