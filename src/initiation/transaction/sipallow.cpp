#include "sipallow.h"

SIPAllow::SIPAllow():
  ourSupported_(std::shared_ptr<QList<SIPRequestMethod>>(new QList<SIPRequestMethod>))
{
  ourSupported_->push_back(SIP_INVITE);
  ourSupported_->push_back(SIP_ACK);
  ourSupported_->push_back(SIP_BYE);
  ourSupported_->push_back(SIP_CANCEL);
}


void SIPAllow::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  if (((response.message->cSeq.method == SIP_INVITE ||
        response.message->cSeq.method == SIP_OPTIONS) &&
       200 <= response.type &&  response.type <= 299) ||
      response.type == 405)
  {
    response.message->allow = ourSupported_;
  }

  emit outgoingResponse(response, content);
}


void SIPAllow::processIncomingRequest(SIPRequest& request, QVariant& content,
                            SIPResponseStatus generatedResponse)
{
  if (generatedResponse == SIP_NO_RESPONSE)
  {
    if (!ourSupported_->contains(request.method))
    {
      generatedResponse = SIP_NOT_ALLOWED;
    }
  }

  emit incomingRequest(request, content, generatedResponse);
}
