#include "siphelper.h"

#include "common.h"

#include <QSettings>

const uint32_t BRANCHLENGTH = 32 - 7;
const uint32_t CALLIDLENGTH = 16;

SIPHelper::SIPHelper():
  localUri_(),
  remoteUri_(),
  initiated_(false)
{}

void SIPHelper::initServer(SIP_URI remoteUri)
{
  Q_ASSERT(!initiated_);

  initLocalURI();
  initiated_ = true;
}

void SIPHelper::initPeerToPeer(SIP_URI remoteUri)
{
  Q_ASSERT(!initiated_);

  initLocalURI();
  remoteUri_ = remoteUri;
  initiated_ = true;
}

void SIPHelper::initLocalURI()
{
  // init stuff from the settings
  QSettings settings;

  localUri_.realname = settings.value("local/Name").toString();
  localUri_.username = settings.value("local/Username").toString();

  if(localUri_.username.isEmpty())
  {
    localUri_.username = "anonymous";
  }

  // TODO: Get server URI from settings
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

bool SIPHelper::isAllowedUser(SIP_URI user)
{
  qDebug() << "ERROR: Block lists not implemented!";
  return false;
}

std::shared_ptr<SIPMessageInfo> SIPHelper::generateMessageBase(QString localAddress)
{
  Q_ASSERT(localUri_.username != "" && remoteUri_.username != "");
  if(localUri_.username == "" || remoteUri_.username == "")
  {
    qWarning() << "ERROR: SIP Helper has not been initialized";
    return std::shared_ptr<SIPMessageInfo>(NULL);
  }

  std::shared_ptr<SIPMessageInfo> mesg = std::shared_ptr<SIPMessageInfo>(new SIPMessageInfo);
  mesg->version = "2.0";
  mesg->maxForwards = 71;

  mesg->dialog = NULL;

  mesg->from = localUri_;
  mesg->to = remoteUri_;
  mesg->contact = localUri_;

  mesg->cSeq = 0;

  mesg->senderReplyAddress.push_back(getLocalVia(localAddress));

  return mesg;
}


void SIPHelper::generateNonDialogRequest(std::shared_ptr<SIPMessageInfo> messageBase)
{
  messageBase->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{"", "", generateRandomString(CALLIDLENGTH)});
}
