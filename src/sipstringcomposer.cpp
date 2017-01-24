#include "sipstringcomposer.h"

SIPStringComposer::SIPStringComposer()
{

}


messageID SIPStringComposer::startRequest(const Request request, const QString& SIPversion)
{
  qDebug() << "Composing message number:" << messages_.size() + 1;
  messages_.push_back(new SIPMessage);

  switch(request)
  {
  case INVITE:
  {
    messages_.back()->request = "INVITE";
    break;
  }
  default:
  {
    qCritical() << "SIP REQUEST NOT IMPLEMENTED";
    messages_.back()->request = "UNKNOWN";
    break;
  }
  }

  qCritical() << "NOT IMPLEMENTED";

  return messages_.size();
}


messageID SIPStringComposer::startResponse(const Response reponse, const QString& SIPversion)
{
  qCritical() << "NOT IMPLEMENTED";
}

// include their tag only if it was already provided
void SIPStringComposer::to(messageID id, QString& username, const QHostInfo& hostname, const QString& tag)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::toIP(messageID id, QString& username, const QHostAddress& address, const QString& tag)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::from(messageID id, QString& username, const QHostInfo& hostname, const QString& tag)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::fromIP(messageID id, QString& username, const QHostAddress& address, const QString& tag)
{
  qCritical() << "NOT IMPLEMENTED";
}

// Where to send responses. branch is generated. Also adds the contact field with same info.
void SIPStringComposer::via(messageID id, const QString& hostname)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::viaIP(messageID id, QHostAddress address)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::maxForwards(messageID id, uint32_t forwards)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::setCallID(messageID id, QString& callID)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::sequenceNum(messageID id, uint32_t seq)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::addSDP(messageID id, QString& sdp)
{
  qCritical() << "NOT IMPLEMENTED";
}

// returns the complete SIP message if successful.
QString& SIPStringComposer::composeMessage(messageID id)
{
  qCritical() << "NOT IMPLEMENTED";
}

void SIPStringComposer::removeMessage(messageID id)
{
  qCritical() << "NOT IMPLEMENTED";
}
