#include "sipnondialogclient.h"

#include <QDebug>


SIPNonDialogClient::SIPNonDialogClient(SIPTransactionUser *tu): SIPClientTransaction (tu)
{}

void SIPNonDialogClient::set_remoteURI(SIP_URI& uri)
{
  remoteUri_ = uri;
}

bool SIPNonDialogClient::processResponse(SIPResponse& response)
{
  // TODO
  Q_UNUSED(response);
  if (getOngoingRequest() == SIP_REGISTER)
  {
    qDebug() << "Got a response for REGISTER! TODO: Processing not implemented!";
  }
  return false;
}

void SIPNonDialogClient::sendRequest(RequestType type)
{
  emit sendNonDialogRequest(remoteUri_, type);
  SIPClientTransaction::sendRequest(type);
}

void SIPNonDialogClient::registerToServer()
{
  sendRequest(SIP_REGISTER);
}
