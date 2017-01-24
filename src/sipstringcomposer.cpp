#include "sipstringcomposer.h"

SIPStringComposer::SIPStringComposer()
{

}


messageID SIPStringComposer::startSIPString(const MessageType message, const QString& SIPversion)
{
  qDebug() << "Composing message number:" << messages_.size() + 1;
  messages_.push_back(new SIPMessage);

  switch(message)
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

  messages_.back()->version = SIPversion;

  return messages_.size();
}


// include their tag only if it was already provided
void SIPStringComposer::to(messageID id, QString& username, const QHostInfo& hostname, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->theirName = username;
  messages_.at(id - 1)->theirLocation = hostname.hostName();
  messages_.at(id - 1)->theirTag = tag;
}

void SIPStringComposer::toIP(messageID id, QString& username, const QHostAddress& address, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->theirName = username;
  messages_.at(id - 1)->theirLocation = address.toString();
  messages_.at(id - 1)->theirTag = tag;
}

void SIPStringComposer::from(messageID id, QString& username, const QHostInfo& hostname, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->ourName = username;
  messages_.at(id - 1)->ourLocation = hostname.hostName();
  messages_.at(id - 1)->ourTag = tag;
}

void SIPStringComposer::fromIP(messageID id, QString& username, const QHostAddress& address, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->ourName = username;
  messages_.at(id - 1)->ourLocation = address.toString();
  messages_.at(id - 1)->ourTag = tag;
}

// Where to send responses. branch is generated. Also adds the contact field with same info.
void SIPStringComposer::via(messageID id, const QHostInfo& hostname)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->replyAddress = hostname.hostName();
}

void SIPStringComposer::viaIP(messageID id, QHostAddress address)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->replyAddress = address.toString();
}

void SIPStringComposer::maxForwards(messageID id, uint32_t forwards)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->maxForwards = forwards;
}

void SIPStringComposer::setCallID(messageID id, QString& callID)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->callID = callID;
}

void SIPStringComposer::sequenceNum(messageID id, uint32_t seq)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  Q_ASSERT(seq != 0);

  QString num;
  messages_.at(id - 1)->cSeq = num.setNum(seq);
}

void SIPStringComposer::addSDP(messageID id, QString& sdp)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  messages_.at(id - 1)->contentType = "application/sdp";
  messages_.at(id - 1)->contentLength = sdp.length();
  messages_.at(id - 1)->content = sdp;
}

// returns the complete SIP message if successful.
QString SIPStringComposer::composeMessage(messageID id)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  qCritical() << "NOT IMPLEMENTED";

  return "empty";
}

void SIPStringComposer::removeMessage(messageID id)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  qCritical() << "NOT IMPLEMENTED";
}
