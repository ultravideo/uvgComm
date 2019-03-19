#include "sipregistration.h"

#include "common.h"

#include <QSettings>

#include <QDateTime>


SIPRegistration::SIPRegistration(QString serverAddress):
  localUri_(),
  serverAddress_(serverAddress),
  registerCSeq_(QDateTime::currentSecsSinceEpoch()%2147483647)
{}

void SIPRegistration::initServer(quint32 transportID)
{
  Q_ASSERT(transportID);

  initLocalURI();
  transportID_ = transportID;
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

  localUri_.host = serverAddress_;
}

ViaInfo SIPRegistration::getLocalVia(QString localAddress)
{
  return ViaInfo{TCP, "2.0", localAddress, QString("z9hG4bK" + generateRandomString(BRANCHLENGTH))};
}

bool SIPRegistration::isContactAtThisServer(QString serverAddress)
{
  return serverAddress == serverAddress_;
}

void SIPRegistration::fillRegisterRequest(std::shared_ptr<SIPMessageInfo>& message,
                                          QString localAddress)
{
  Q_ASSERT(message);
  Q_ASSERT(localUri_.username != "");
  if (message == nullptr)
  {
    qWarning() << "ERROR: Message has not been initialized before filling in SIP regisgtration";
  }
  if (localUri_.username == "")
  {
    qWarning() << "ERROR: SIP Helper has not been initialized";
    return;
  }

  message->from = localUri_;
  message->to = localUri_;
  message->contact = localUri_;
  message->cSeq = registerCSeq_;
  message->senderReplyAddress.push_back(getLocalVia(localAddress));

  ++registerCSeq_;
}
