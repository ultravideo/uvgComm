#include "sipregistration.h"

#include "serverstatusview.h"

#include "common.h"
#include "global.h"
#include "logger.h"

#include <QVariant>

const int REGISTER_SEND_PERIOD = (REGISTER_INTERVAL - 5)*1000;


SIPRegistration::SIPRegistration():
  active_(false),
  retryTimer_(nullptr)
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
  if (active_)
  {
    sendREGISTERRequest(0);
  }

  Logger::getLogger()->printNormal(this, "Finished uniniating registration");
  return;
}


void SIPRegistration::bindToServer(NameAddr& addressRecord, QString localAddress,
                                    uint16_t port)
{
  Logger::getLogger()->printNormal(this, "Binding to server", {"Server"},
                                   {addressRecord.uri.hostport.host});

  active_ = false;
  serverAddress_ = addressRecord.uri.hostport.host;
  statusView_->updateServerStatus("Request sent. Waiting response...");

  sendREGISTERRequest(REGISTER_INTERVAL);
}


void SIPRegistration::processIncomingResponse(SIPResponse& response, QVariant& content,
                                              bool retryRequest)
{
  Q_UNUSED(content);
  // REGISTER response must not create route. In other words ignore all record-routes

  if (retryRequest)
  {
    sendREGISTERRequest(REGISTER_INTERVAL);
    return;
  }

  if (!response.message->contact.empty())
  {
    active_ = true;
    statusView_->updateServerStatus("Registered");
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

  if (active_)
  {
    statusView_->updateServerStatus("Second request sent. Waiting response...");
    sendREGISTERRequest(REGISTER_INTERVAL);
  }
}


bool SIPRegistration::haveWeRegistered()
{
  return active_;
}


void SIPRegistration::sendREGISTERRequest(uint32_t expires)
{
  SIPRequest request;
  request.method = SIP_REGISTER;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
  request.message->expires = std::shared_ptr<uint32_t> (new uint32_t{expires});

  QVariant content;
  emit outgoingRequest(request, content);
}
