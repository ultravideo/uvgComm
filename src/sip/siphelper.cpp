#include "siphelper.h"

#include "common.h"

#include <QSettings>

const uint32_t BRANCHLENGTH = 32 - 7;

SIPHelper::SIPHelper()
{

}

void SIPHelper::init()
{
  // init stuff from the settings
  QSettings settings;

  localUri_.realname = settings.value("local/Name").toString();
  localUri_.username = settings.value("local/Username").toString();


  if(localUri_.username.isEmpty())
  {
    localUri_.username = "anonymous";
  }

  // TODO: Get server URI
  localUri_.host = "";
}

QString SIPHelper::getServerLocation()
{
  return "";
}

ViaInfo SIPHelper::getLocalVia(QString localAddress)
{
  return ViaInfo{TCP, "2.0", localAddress, QString("z9hG4bK" + generateRandomString(BRANCHLENGTH))};
}

// use set/get remote URI when initializing a dialog
void SIPHelper::setNextRemoteURI(SIP_URI remoteUri)
{
  nextRemoteUri_ = remoteUri;
}

SIP_URI SIPHelper::getNextRemoteURI()
{
  return nextRemoteUri_;
}

SIP_URI SIPHelper::getLocalURI()
{
  return localUri_;
}

SIP_URI SIPHelper::getLocalContactURI()
{
  return localUri_;
}

bool SIPHelper::isAllowedUser(SIP_URI user)
{
  qDebug() << "ERROR: Block lists not implemented!";
  return false;
}

std::shared_ptr<SIPMessageInfo> SIPHelper::getMessageBase()
{
  std::shared_ptr<SIPMessageInfo> mesg = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  mesg->version = "2.0";
  mesg->cSeq = 0;

  return mesg;
}
