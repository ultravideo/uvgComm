#include "sipstringcomposer.h"

SIPStringComposer::SIPStringComposer()
{}



QString SIPStringComposer::requestToString(const RequestType request)
{
  switch(request)
  {
    case INVITE:
    {
      return "INVITE";
      break;
    }
    case ACK:
    {
      return "ACK";
      break;
    }
    case BYE:
    {
      return "BYE";
      break;
    }
    case NOREQUEST:
    {
      qCritical() << "WARNING: Received NOREQUEST for string translation";
      break;
    }
    default:
    {
      qCritical() << "WARNING: SIP REQUEST NOT IMPLEMENTED";
      return "";
      break;
    }
  }
  return "";
}

QString SIPStringComposer::responseToString(const ResponseType response)
{
  switch(response)
  {
    case RINGING_180:
    {
      return "180 RINGING";
      break;
    }
    case OK_200:
    {
      return "200 OK";
      break;
    }
    case DECLINE_603:
    {
      return "603 DECLINE";
      break;
    }
    case NORESPONSE:
    {
      qCritical() << "Received NORESPONSE for string translation";
      break;
    }
    default:
    {
      qCritical() << "SIP RESPONSE NOT IMPLEMENTED";
      break;
    }
  }
  return "";
}

void SIPStringComposer::initializeMessage(const QString& SIPversion)
{
  qDebug() << "Composing message number:" << messages_.size() + 1;

  if(SIPversion != "2.0")
  {
    qWarning() << "Unsupported SIP version:" << SIPversion;
  }

  messages_.push_back(new SIPMessage);
  messages_.back()->version = SIPversion;
}

messageID SIPStringComposer::startSIPRequest(const RequestType request, const QString& SIPversion)
{
  initializeMessage(SIPversion);
  messages_.back()->method = requestToString(request);
  messages_.back()->isRequest = true;
  return messages_.size();
}

messageID SIPStringComposer::startSIPResponse(const ResponseType response, const QString& SIPversion)
{
  initializeMessage(SIPversion);
  messages_.back()->method = responseToString(response);
  messages_.back()->isRequest = false;
  return messages_.size();
}

// include their tag only if it was already provided
void SIPStringComposer::to(messageID id, QString& name, QString &username, const QString &hostname, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  Q_ASSERT(!name.isEmpty() && !username.isEmpty());

  messages_.at(id - 1)->remoteName = name;
  messages_.at(id - 1)->remoteUsername = username;
  messages_.at(id - 1)->remoteLocation = hostname;
  messages_.at(id - 1)->remoteTag = tag;
}

void SIPStringComposer::from(messageID id, QString& name, QString& username, const QHostInfo& hostname, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->localName = name;
  messages_.at(id - 1)->localUsername = username;
  messages_.at(id - 1)->localLocation = hostname.hostName();
  messages_.at(id - 1)->localTag = tag;
}

void SIPStringComposer::fromIP(messageID id, QString& name, QString &username, const QHostAddress& address, const QString& tag)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  messages_.at(id - 1)->localName = name;
  messages_.at(id - 1)->localUsername = username;
  messages_.at(id - 1)->localLocation = address.toString();
  messages_.at(id - 1)->localTag = tag;
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

void SIPStringComposer::sequenceNum(messageID id, uint32_t seq, const RequestType originalRequest)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);
  Q_ASSERT(seq != 0);
  Q_ASSERT(originalRequest != NOREQUEST);

  QString num;
  messages_.at(id - 1)->cSeq = num.setNum(seq);

  messages_.at(id - 1)->originalRequest = requestToString(originalRequest);
}

void SIPStringComposer::addSDP(messageID id, QString& sdp)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  if(sdp.isEmpty())
  {
    qWarning() << "WARNING: Tried composing a message with empty SDP message";
    return;
  }

  messages_.at(id - 1)->contentType = "application/sdp";
  QString num;
  messages_.at(id - 1)->contentLength = num.setNum(sdp.length());
  messages_.at(id - 1)->content = sdp;
}

// returns the complete SIP message if successful.
QString SIPStringComposer::composeMessage(messageID id)
{
  Q_ASSERT(messages_.size() >= id && messages_.at(id - 1) != 0);

  // check existance of required fields
  if(messages_.at(id - 1)->method.isEmpty() ||
     messages_.at(id - 1)->version.isEmpty() ||
     messages_.at(id - 1)->remoteName.isEmpty() ||
     messages_.at(id - 1)->remoteUsername.isEmpty() ||
     messages_.at(id - 1)->remoteLocation.isEmpty() ||
     messages_.at(id - 1)->maxForwards.isEmpty() ||
     messages_.at(id - 1)->localName.isEmpty() ||
     messages_.at(id - 1)->localUsername.isEmpty() ||
     messages_.at(id - 1)->localLocation.isEmpty() ||
     messages_.at(id - 1)->replyAddress.isEmpty() ||
     messages_.at(id - 1)->localTag.isEmpty() ||
     messages_.at(id - 1)->callID.isEmpty() ||
     messages_.at(id - 1)->host.isEmpty() ||
     messages_.at(id - 1)->cSeq.isEmpty() ||
     messages_.at(id - 1)->originalRequest.isEmpty() ||
     messages_.at(id - 1)->branch.isEmpty())
  {
    qWarning() << "WARNING: All required SIP fields have not been provided";
    qWarning() << messages_.at(id - 1)->method <<
                  messages_.at(id - 1)->version <<
                  messages_.at(id - 1)->remoteName <<
                  messages_.at(id - 1)->remoteUsername <<
                  messages_.at(id - 1)->remoteLocation <<
                  messages_.at(id - 1)->maxForwards <<
                  messages_.at(id - 1)->localName <<
                  messages_.at(id - 1)->localUsername <<
                  messages_.at(id - 1)->localLocation <<
                  messages_.at(id - 1)->replyAddress <<
                  messages_.at(id - 1)->localTag <<
                  messages_.at(id - 1)->callID <<
                  messages_.at(id - 1)->host <<
                  messages_.at(id - 1)->cSeq <<
                  messages_.at(id - 1)->originalRequest <<
                  messages_.at(id - 1)->branch;

    return QString();
  }

  if(messages_.at(id - 1)->remoteTag.isEmpty())
  {
    qDebug() << "Their tag will be provided later.";
  }

  if(messages_.at(id - 1)->contentType.isEmpty())
  {
    qDebug() << "No SDP has been provided for SIP message. Not including body";
  }

  QString lineEnding = "\r\n";
  QString message = "";

  if(messages_.at(id - 1)->isRequest)
  {
    // INVITE sip:bob@biloxi.com SIP/2.0
    message = messages_.at(id - 1)->method
        + " sip:" + messages_.at(id - 1)->remoteUsername + "@" + messages_.at(id - 1)->remoteLocation
        + " SIP/" + messages_.at(id - 1)->version + lineEnding;
  }
  else
  {
    // response
    message = "SIP/" + messages_.at(id - 1)->version + " " + messages_.at(id - 1)->method + lineEnding;
  }

  message += "Via: SIP/" + messages_.at(id - 1)->version + "/UDP " + messages_.at(id - 1)->replyAddress
      + ";branch=" + messages_.at(id - 1)->branch + lineEnding;

  message += "Max-Forwards: " + messages_.at(id - 1)->maxForwards + lineEnding;
  message += "To: " + messages_.at(id - 1)->remoteName
      + " <sip:" + messages_.at(id - 1)->remoteUsername
      + "@" + messages_.at(id - 1)->remoteLocation + ">";

  if(!messages_.at(id - 1)->remoteTag.isEmpty())
  {
    message += ";tag=" + messages_.at(id - 1)->remoteTag;
  }

  message += lineEnding;
  message += "From: " + messages_.at(id - 1)->localName
      + " <sip:" + messages_.at(id - 1)->localUsername
      + "@" + messages_.at(id - 1)->localLocation + ">";

  if(!messages_.at(id - 1)->localTag.isEmpty())
  {
    message += ";tag=" + messages_.at(id - 1)->localTag;
  }
  message += lineEnding;
  message += "Call-ID: " + messages_.at(id - 1)->callID + "@" + messages_.at(id - 1)->host + lineEnding;
  message += "CSeq: " + messages_.at(id - 1)->cSeq + " " + messages_.at(id - 1)->originalRequest + lineEnding;
  message += "Contact: <sip:" + messages_.at(id - 1)->localUsername
      + "@" + messages_.at(id - 1)->localLocation + ">" + lineEnding;

  if(!messages_.at(id - 1)->contentType.isEmpty() &&
     !messages_.at(id - 1)->contentLength.isEmpty())
  {
    message += "Content-Type: " + messages_.at(id - 1)->contentType + lineEnding;
    message += "Content-Length: " + messages_.at(id - 1)->contentLength + lineEnding;
  }
  else
  {
    message += "Content-Length: 0" + lineEnding;
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

QString SIPStringComposer::formSDP(const std::shared_ptr<SDPMessageInfo> sdpInfo)
{
  if(sdpInfo == NULL ||
     sdpInfo->version != 0 ||
     sdpInfo->originator_username.isEmpty() ||
     sdpInfo->host_nettype.isEmpty() ||
     sdpInfo->host_addrtype.isEmpty() ||
     sdpInfo->sessionName.isEmpty() ||
     sdpInfo->host_address.isEmpty() ||
     sdpInfo->connection_nettype.isEmpty() ||
     sdpInfo->connection_addrtype.isEmpty() ||
     sdpInfo->connection_address.isEmpty() ||
     sdpInfo->media.empty()
     )
  {

    if(sdpInfo != NULL)
    {
      qCritical() << "WARNING: Bad SDPInfo in string formation";
      qWarning() << sdpInfo->version <<
          sdpInfo->originator_username <<
          sdpInfo->host_nettype <<
          sdpInfo->host_addrtype <<
          sdpInfo->sessionName <<
          sdpInfo->host_address <<
          sdpInfo->connection_nettype <<
          sdpInfo->connection_addrtype <<
          sdpInfo->connection_address;
    }
    else
    {
      qCritical() << "WARNING: Tried to form SDP message with null pointer";
    }
    return "";
  }

  QString sdp = "";
  QString lineEnd = "\r\n";
  sdp += "v=" + QString::number(sdpInfo->version) + lineEnd;
  sdp += "o=" + sdpInfo->originator_username + " " + QString::number(sdpInfo->sess_id)  + " "
      + QString::number(sdpInfo->sess_v) + " " + sdpInfo->host_nettype + " "
      + sdpInfo->host_addrtype + " " + sdpInfo->host_address + lineEnd;

  sdp += "s=" + sdpInfo->sessionName + lineEnd;
  sdp += "c=" + sdpInfo->connection_nettype + " " + sdpInfo->connection_addrtype +
      + " " + sdpInfo->connection_address + " " + lineEnd;
  sdp += "t=" + QString::number(sdpInfo->startTime) + " "
      + QString::number(sdpInfo->endTime) + lineEnd;

  for(auto mediaStream : sdpInfo->media)
  {
    sdp += "m=" + mediaStream.type + " " + QString::number(mediaStream.receivePort)
        + " " + mediaStream.proto + " " + QString::number(mediaStream.rtpNum)
        + lineEnd;
    if(!mediaStream.nettype.isEmpty())
    {
      sdp += "c=" + mediaStream.nettype + " " + mediaStream.addrtype + " "
          + mediaStream.address + lineEnd;
    }

    for(auto rtpmap : mediaStream.codecs)
    {
      sdp += "a=rtpmap:" + QString::number(rtpmap.rtpNum) + " "
          + rtpmap.codec + "/" + QString::number(rtpmap.clockFrequency) + lineEnd;
    }
  }

  qDebug().noquote() << "Generated SDP string:" << sdp;
  return sdp;
}

