#include "sipregistration.h"

#include "common.h"

#include <QSettings>


SIPRegistration::SIPRegistration(QString serverAddress):
  localUri_(),
  initiated_(false)
{}

void SIPRegistration::initServer()
{
  Q_ASSERT(!initiated_);

  initLocalURI();
  initiated_ = true;
}

void SIPRegistration::initLocalURI()
{
  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  localUri_.realname = settings.value("local/Name").toString();
  localUri_.username = settings.value("local/Username").toString();

  if(localUri_.username.isEmpty())
  {
    localUri_.username = "anonymous";
  }

  // TODO: Get server URI from settings
  localUri_.host = "";
}

void SIPRegistration::setHost(QString location)
{
  localUri_.host = location;
}

ViaInfo SIPRegistration::getLocalVia(QString localAddress)
{
  return ViaInfo{TCP, "2.0", localAddress, QString("z9hG4bK" + generateRandomString(BRANCHLENGTH))};
}

bool SIPRegistration::isContactAtThisServer(QString serverAddress)
{
  return serverAddress == serverAddress_;
}

std::shared_ptr<SIPMessageInfo> SIPRegistration::generateRegisterRequest(QString localAddress)
{
  Q_ASSERT(localUri_.username != "");
  if(localUri_.username == "")
  {
    qWarning() << "ERROR: SIP Helper has not been initialized";
    return std::shared_ptr<SIPMessageInfo>(nullptr);
  }

  std::shared_ptr<SIPMessageInfo> mesg = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  mesg->version = "2.0";
  mesg->maxForwards = 71;
  mesg->dialog = nullptr;
  mesg->from = localUri_;
  mesg->to = localUri_;
  mesg->contact = localUri_;
  mesg->cSeq = 1;
  mesg->senderReplyAddress.push_back(getLocalVia(localAddress));
  mesg->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{"", "", generateRandomString(16)});

  return mesg;
}
