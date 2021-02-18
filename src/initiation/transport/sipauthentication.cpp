#include "sipauthentication.h"

#include "common.h"

SIPAuthentication::SIPAuthentication()
{

}


void SIPAuthentication::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  printNormal(this, "Processing outgoing request");

  if (request.method == SIP_REGISTER)
  {
    request.message->authorization = responses_;
  }
  else
  {
    request.message->proxyAuthorization = responses_;
  }

  emit outgoingRequest(request, content);
}


void SIPAuthentication::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  printNormal(this, "Processing incoming response");

  // I think this is the REGISTER authorization
  if (response.type == SIP_UNAUTHORIZED ||
      response.type == SIP_PROXY_AUTHENTICATION_REQUIRED)
  {
    // it may be that these present a different challenge in which case these have to be done separately


    // CALCULATE ALL RESPONSES HERE AND PUT IN responses_

    printUnimplemented(this, "Calculating authentication response");

  }

  emit incomingResponse(response, content);
}
