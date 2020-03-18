#include "sipnondialogclient.h"

#include "global.h"


SIPNonDialogClient::SIPNonDialogClient():
  SIPClient (),
  expires_(REGISTER_INTERVAL)
{}

void SIPNonDialogClient::set_remoteURI(SIP_URI& uri)
{
  remoteUri_ = uri;
}

void SIPNonDialogClient::getRequestMessageInfo(RequestType type,
                                               std::shared_ptr<SIPMessageInfo>& outMessage)
{
  SIPClient::getRequestMessageInfo(type, outMessage);

  if (type == SIP_REGISTER)
  {
    outMessage->expires = expires_;

    if (!outMessage->vias.empty())
    {
      outMessage->vias.back().alias = true;
      outMessage->vias.back().rport = true;
    }
  }
}


bool SIPNonDialogClient::processResponse(SIPResponse& response,
                                         SIPDialogState &state)
{
  return SIPClient::processResponse(response, state);
}


void SIPNonDialogClient::registerToServer()
{
  expires_ = REGISTER_INTERVAL;
  if (startTransaction(SIP_REGISTER))
  {
    emit sendNondialogRequest(remoteUri_, SIP_REGISTER);
  }
}


void SIPNonDialogClient::unRegister()
{
  expires_ = 0;
  if (startTransaction(SIP_REGISTER))
  {
    emit sendNondialogRequest(remoteUri_, SIP_REGISTER);
  }
}
