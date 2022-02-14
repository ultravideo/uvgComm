#pragma once

#include <initiation/sipmessageprocessor.h>

/* This processor calls callbacks with all incoming requests and responses */

class SIPCallbacks : public SIPMessageProcessor
{
public:

  // for dialog callbacks
  SIPCallbacks(uint32_t sessionID,
               std::vector<std::function<void(uint32_t sessionID,
                                              SIPRequest& request,
                                              QVariant& content)>>& requestCallbacks,
               std::vector<std::function<void(uint32_t sessionID,
                                              SIPResponse& response,
                                              QVariant& content)>>& responseCallbacks);
  // for server callbacks
  SIPCallbacks(QString address,
               std::vector<std::function<void(QString address,
                                              SIPRequest& request,
                                              QVariant& content)>>& requestCallbacks,
               std::vector<std::function<void(QString address,
                                              SIPResponse& response,
                                              QVariant& content)>>& responseCallbacks);

  void installSIPRequestCallback(
      std::function<void(uint32_t sessionID,
                         SIPRequest& request,
                         QVariant& content)> callback);
  void installSIPResponseCallback(
      std::function<void(uint32_t sessionID,
                         SIPResponse& response,
                         QVariant& content)> callback);

  void installSIPRequestCallback(
      std::function<void(QString address,
                         SIPRequest& request,
                         QVariant& content)> callback);
  void installSIPResponseCallback(
      std::function<void(QString address,
                         SIPResponse& response,
                         QVariant& content)> callback);

  void clearCallbacks();

public slots:

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                      SIPResponseStatus generatedResponse);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

private:

  std::vector<std::function<void(uint32_t sessionID,
                                 SIPRequest& request,
                                 QVariant& content)>> requestCallbacks_;
  std::vector<std::function<void(uint32_t sessionID,
                                 SIPResponse& response,
                                 QVariant& content)>> responseCallbacks_;

  uint32_t sessionID_;

  std::vector<std::function<void(QString address,
                                 SIPRequest& request,
                                 QVariant& content)>> registrationsRequests_;

  std::vector<std::function<void(QString address,
                                 SIPResponse& response,
                                 QVariant& content)>> registrationResponses_;

  QString address_;


};

