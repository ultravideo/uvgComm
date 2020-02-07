#include "sipnondialogclient.h"

#include "global.h"

#include <QDebug>


SIPNonDialogClient::SIPNonDialogClient(SIPTransactionUser *tu): SIPClientTransaction (tu)
{}

void SIPNonDialogClient::set_remoteURI(SIP_URI& uri)
{
  remoteUri_ = uri;
}

void SIPNonDialogClient::getRequestMessageInfo(RequestType type,
                                               std::shared_ptr<SIPMessageInfo>& outMessage)
{
  SIPClientTransaction::getRequestMessageInfo(type, outMessage);

  if (type == SIP_REGISTER)
  {
    outMessage->expires = REGISTER_INTERVAL;

    if (!outMessage->vias.empty())
    {
      outMessage->vias.back().alias = true;
      outMessage->vias.back().rport = true;
    }
  }
}


bool SIPNonDialogClient::processResponse(SIPResponse& response,
                                         std::shared_ptr<SIPDialogState> state)
{
  return SIPClientTransaction::processResponse(response, state);
}


void SIPNonDialogClient::startTransaction(RequestType type)
{
  SIPClientTransaction::startTransaction(type);
}


void SIPNonDialogClient::registerToServer()
{
  startTransaction(SIP_REGISTER);
  emit sendNondialogRequest(remoteUri_, SIP_REGISTER);
}
