#include "sipcallbacks.h"


SIPCallbacks::SIPCallbacks(uint32_t sessionID,
             std::vector<std::function<void(uint32_t sessionID,
                                            SIPRequest& request,
                                            QVariant& content)>>& requestCallbacks,
             std::vector<std::function<void(uint32_t sessionID,
                                            SIPResponse& response,
                                            QVariant& content)>>& responseCallbacks):
  requestCallbacks_(requestCallbacks),
  responseCallbacks_(responseCallbacks),
  sessionID_(sessionID),
  registrationsRequests_(),
  registrationResponses_(),
  address_("")
{}

SIPCallbacks::SIPCallbacks(QString address,
             std::vector<std::function<void(QString address,
                                            SIPRequest& request,
                                            QVariant& content)>>& requestCallbacks,
             std::vector<std::function<void(QString address,
                                            SIPResponse& response,
                                            QVariant& content)>>& responseCallbacks):
  requestCallbacks_(),
  responseCallbacks_(),
  sessionID_(),
  registrationsRequests_(requestCallbacks),
  registrationResponses_(responseCallbacks),
  address_(address)
{}

void SIPCallbacks::installSIPRequestCallback(
    std::function<void(uint32_t sessionID,
                       SIPRequest& request,
                       QVariant& content)> callback)
{
  requestCallbacks_.push_back(callback);
}


void SIPCallbacks::installSIPResponseCallback(
    std::function<void(uint32_t sessionID,
                       SIPResponse& response,
                       QVariant& content)> callback)
{
  responseCallbacks_.push_back(callback);
}

void SIPCallbacks::installSIPRequestCallback(
    std::function<void(QString address,
                       SIPRequest& request,
                       QVariant& content)> callback)
{
  registrationsRequests_.push_back(callback);
}


void SIPCallbacks::installSIPResponseCallback(
    std::function<void(QString address,
                       SIPResponse& response,
                       QVariant& content)> callback)
{
  registrationResponses_.push_back(callback);
}


void SIPCallbacks::clearCallbacks()
{
  requestCallbacks_.clear();
  responseCallbacks_.clear();
  registrationsRequests_.clear();
  registrationResponses_.clear();
}


void SIPCallbacks::processIncomingRequest(SIPRequest& request, QVariant& content,
                                          SIPResponseStatus generatedResponse)
{
  Q_UNUSED(generatedResponse);
  for (auto& callback : requestCallbacks_)
  {
    callback(sessionID_, request, content);
  }

  for (auto& callback : registrationsRequests_)
  {
    callback(address_, request, content);
  }

  // requests stop at callbacks
}


void SIPCallbacks::processIncomingResponse(SIPResponse& response, QVariant& content,
                                           bool retryRequest)
{
  for (auto& callback : responseCallbacks_)
  {
    callback(sessionID_, response, content);
  }

  for (auto& callback : registrationResponses_)
  {
    callback(address_, response, content);
  }

  // responses stop at callbacks
}
