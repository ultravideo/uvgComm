#pragma once

#include "initiation/sipmessageprocessor.h"

#include "initiation/siptypes.h"

class SIPAuthentication : public SIPMessageProcessor
{
  Q_OBJECT
public:
  SIPAuthentication();

public slots:

  // add credentials to request, if we have them
  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);

  // take challenge if they require authentication
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content);


private:

  QList<DigestResponse> responses_;
};
