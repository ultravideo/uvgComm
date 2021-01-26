#include "sipnondialogclient.h"

#include "global.h"


SIPNonDialogClient::SIPNonDialogClient():
  SIPClient (),
  expires_(REGISTER_INTERVAL)
{}


SIPNonDialogClient::~SIPNonDialogClient()
{}


void SIPNonDialogClient::set_remoteURI(SIP_URI& uri)
{
  remoteUri_ = uri;
}

void SIPNonDialogClient::getRequestMessageInfo(SIPRequestMethod type,
                                               std::shared_ptr<SIPMessageHeader>& outMessage)
{
  SIPClient::getRequestMessageInfo(type, outMessage);

  if (type == SIP_REGISTER)
  {
    outMessage->expires = std::shared_ptr<uint32_t> (new uint32_t{expires_});
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
