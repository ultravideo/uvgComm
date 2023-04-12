#pragma once
#include "../sipmessageprocessor.h"



class SIPAllow : public SIPMessageProcessor
{
public:
  SIPAllow();

public slots:

  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                      SIPResponseStatus generatedResponse);

private:

  std::shared_ptr<QList<SIPRequestMethod>> ourSupported_;
};
