#include "sipnondialogclient.h"

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
    outMessage->expires = 600; // TODO: Implement resending
  }
}


bool SIPNonDialogClient::processResponse(SIPResponse& response,
                                         std::shared_ptr<SIPDialogState> state)
{
  // TODO
  Q_UNUSED(response);
  Q_UNUSED(state);

  if (getOngoingRequest() == SIP_REGISTER)
  {
    qDebug() << "Got a response for REGISTER! TODO: Processing not implemented!";
  }
  return false;
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
