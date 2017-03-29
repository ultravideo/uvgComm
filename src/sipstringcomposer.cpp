#include "sipstringcomposer.h"

SIPStringComposer::SIPStringComposer():request_(false)
{}

messageID SIPStringComposer::startSIPString(const MessageType message, const QString& SIPversion)
{
  qDebug() << "Composing message number:" << messages_.size() + 1;
  messages_.push_back(new SIPMessage);

  switch(message)
  {
  case INVITE:
  {
    messages_.back()->request = "INVITE";
    request_ = true;
    break;
  }
  case ACK:
  {
    messages_.back()->request = "ACK";
    request_ = true;
    break;
  }
  case BYE:
  {
    messages_.back()->request = "BYE";
    request_ = true;
    break;
  }
  case RINGING_180:
  {
    messages_.back()->request = "180 RINGING";
    request_ = false;
    break;
  }
  case OK_200:
  {
    messages_.back()->request = "200 OK";
    request_ = false;
    break;
  }
  case DECLINE_600:
  {
    messages_.back()->request = "600 DECLINE";
    request_ = false;
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
void SIPStringComposer::to(messageID id, QString& name, QString &username, const QString &hostname, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  Q_ASSERT(!name.isEmpty() && !username.isEmpty());

  messages_.at(id - 1)->theirName = name;
  messages_.at(id - 1)->theirUsername = username;
  messages_.at(id - 1)->theirLocation = hostname;
  messages_.at(id - 1)->theirTag = tag;
}

void SIPStringComposer::from(messageID id, QString& name, QString& username, const QHostInfo& hostname, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->ourName = name;
  messages_.at(id - 1)->ourUsername = username;
  messages_.at(id - 1)->ourLocation = hostname.hostName();
  messages_.at(id - 1)->ourTag = tag;
}

void SIPStringComposer::fromIP(messageID id, QString& name, QString &username, const QHostAddress& address, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->ourName = name;
  messages_.at(id - 1)->ourUsername = username;
  messages_.at(id - 1)->ourLocation = address.toString();
  messages_.at(id - 1)->ourTag = tag;
}

// Where to send responses. branch is generated. Also adds the contact field with same info.
void SIPStringComposer::via(messageID id, const QHostInfo& hostname, QString &branch)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->replyAddress = hostname.hostName();
  messages_.at(id - 1)->branch = branch;
}

void SIPStringComposer::viaIP(messageID id, QHostAddress address, QString& branch)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->replyAddress = address.toString();
  messages_.at(id - 1)->branch = branch;
}

void SIPStringComposer::maxForwards(messageID id, uint16_t forwards)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  QString num;
  messages_.at(id - 1)->maxForwards = num.setNum(forwards);
}

void SIPStringComposer::setCallID(messageID id, QString& callID, QString &host)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->callID = callID;
  messages_.at(id - 1)->host = host;
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
  QString num;
  messages_.at(id - 1)->contentLength = num.setNum(sdp.length());
  messages_.at(id - 1)->content = sdp;
}

// returns the complete SIP message if successful.
QString SIPStringComposer::composeMessage(messageID id)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  // check
  if(messages_.at(id - 1)->request.isEmpty() ||
     messages_.at(id - 1)->version.isEmpty() ||
     messages_.at(id - 1)->theirName.isEmpty() ||
     messages_.at(id - 1)->theirUsername.isEmpty() ||
     messages_.at(id - 1)->theirLocation.isEmpty() ||
     messages_.at(id - 1)->maxForwards.isEmpty() ||
     messages_.at(id - 1)->ourName.isEmpty() ||
     messages_.at(id - 1)->ourUsername.isEmpty() ||
     messages_.at(id - 1)->ourLocation.isEmpty() ||
     messages_.at(id - 1)->replyAddress.isEmpty() ||
     messages_.at(id - 1)->ourTag.isEmpty() ||
     messages_.at(id - 1)->callID.isEmpty() ||
     messages_.at(id - 1)->host.isEmpty() ||
     messages_.at(id - 1)->cSeq.isEmpty() ||
     messages_.at(id - 1)->branch.isEmpty())
  {
    qWarning() << "WARNING: All required SIP fields have not been provided";
    qWarning() << messages_.at(id - 1)->request <<
                  messages_.at(id - 1)->version <<
                  messages_.at(id - 1)->theirName <<
                  messages_.at(id - 1)->theirLocation <<
                  messages_.at(id - 1)->maxForwards <<
                  messages_.at(id - 1)->ourName <<
                  messages_.at(id - 1)->ourLocation <<
                  messages_.at(id - 1)->replyAddress <<
                  messages_.at(id - 1)->ourTag <<
                  messages_.at(id - 1)->callID <<
                  messages_.at(id - 1)->cSeq <<
                  messages_.at(id - 1)->branch;

    return QString();
  }

  if(messages_.at(id - 1)->theirTag.isEmpty())
  {
    qDebug() << "Their tag will be provided later.";
  }

  if(messages_.at(id - 1)->contentType.isEmpty())
  {
    qWarning() << "WARNING: No SDP has been provided for SIP message. Not including body";
  }

  QString lineEnding = "\r\n";
  QString message = "";

  if(request_)
  {
  // INVITE sip:bob@biloxi.com SIP/2.0
  message = messages_.at(id - 1)->request
      + " sip:" + messages_.at(id - 1)->theirUsername + "@" + messages_.at(id - 1)->theirLocation
      + " SIP/" + messages_.at(id - 1)->version + lineEnding;
  }
  else
  {
    // response
    message = messages_.at(id - 1)->version + " " + messages_.at(id - 1)->request + lineEnding;
  }

  message += "Via: SIP/" + messages_.at(id - 1)->version + "/UDP " + messages_.at(id - 1)->replyAddress
      + ";branch=" + messages_.at(id - 1)->branch + lineEnding;

  message += "Max-Forwards: " + messages_.at(id - 1)->maxForwards + lineEnding;

  message += "To: " + messages_.at(id - 1)->theirName
      + " <sip:" + messages_.at(id - 1)->theirUsername
      + "@" + messages_.at(id - 1)->theirLocation + ">";

  if(!messages_.at(id - 1)->theirTag.isEmpty())
  {
    message += ";tag=" + messages_.at(id - 1)->theirTag;
  }

  message += lineEnding;

  message += "From: " + messages_.at(id - 1)->ourName
      + " <sip:" + messages_.at(id - 1)->ourUsername
      + "@" + messages_.at(id - 1)->ourLocation + ">";

  if(!messages_.at(id - 1)->ourTag.isEmpty())
  {
    message += ";tag=" + messages_.at(id - 1)->ourTag;
  }
  message += lineEnding;

  message += "Call-ID: " + messages_.at(id - 1)->callID + "@" + messages_.at(id - 1)->host + lineEnding;
  message += "CSeq: " + messages_.at(id - 1)->cSeq + " " + messages_.at(id - 1)->request + lineEnding;

  message += "Contact: <sip:" + messages_.at(id - 1)->ourUsername
      + "@" + messages_.at(id - 1)->ourLocation + ">" + lineEnding;

  if(!messages_.at(id - 1)->contentType.isEmpty() &&
     !messages_.at(id - 1)->contentLength.isEmpty())
  {
    message += "Content-Type: " + messages_.at(id - 1)->contentType + lineEnding;
    message += "Content-Length: " + messages_.at(id - 1)->contentLength + lineEnding;
  }

  message += lineEnding; // extra line between header and body

  if(!messages_.at(id - 1)->content.isEmpty())
  {
    message += messages_.at(id - 1)->content; // TODO: should there be an end of line?
  }

  qDebug() << "Finished composing SIP message";

  return message;
}

void SIPStringComposer::removeMessage(messageID id)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  qCritical() << "NOT IMPLEMENTED";
}
