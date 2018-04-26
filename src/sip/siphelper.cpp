#include "siphelper.h"

#include <QSettings>

SIPHelper::SIPHelper()
{

}

void SIPHelper::init()
{
  // init stuff from the settings
}

QString SIPHelper::getServerLocation()
{
  return "";
}

ViaInfo SIPHelper::getLocalAddress()
{

}

// use set/get remote URI when initializing a dialog
void SIPHelper::setNextRemoteURI(SIP_URI remoteUri)
{}

SIP_URI SIPHelper::getNextRemoteURI()
{}

SIP_URI SIPHelper::getLocalURI()
{}

SIP_URI SIPHelper::getLocalContactURI()
{}

bool SIPHelper::isForbiddenUser(SIP_URI user)
{}

std::shared_ptr<SIPMessageInfo> SIPHelper::getMessageBase()
{}
