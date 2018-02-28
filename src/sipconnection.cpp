#include "sipconnection.h"

#include "sipconversions.h"
#include "sipfieldparsing.h"
#include "sipfieldcomposing.h"
#include "common.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QList>
#include <QHostInfo>

#include <iostream>
#include <sstream>
#include <string>

#include <functional>

const uint16_t SIP_PORT = 5060;

const std::map<QString, std::function<bool(SIPField& field, std::shared_ptr<SIPMessageInfo>)>> parsing =
{
    {"To", parseToField},
    {"From", parseFromField},
    {"CSeq", parseCSeqField},
    {"Call-ID", parseCallIDField},
    {"Via", parseViaField},
    {"Max-Forwards", parseMaxForwardsField},
    {"Contact", parseContactField},
    {"Content-Type", parseContentTypeField},
    {"Content-Length", parseContentLengthField}
};


SIPConnection::SIPConnection(quint32 sessionID):
  partialMessage_(""),
  messageComposer_(),
  connection_(),
  sessionID_(sessionID)
{}

SIPConnection::~SIPConnection()
{}

void SIPConnection::createConnection(ConnectionType type, QString target)
{
  if(type == TCP)
  {
    qDebug() << "Initiating TCP connection for sip connection number:" << sessionID_;
    connection_ = std::shared_ptr<TCPConnection>(new TCPConnection);
    signalConnections();
    connection_->establishConnection(target, SIP_PORT);
  }
  else
  {
    qDebug() << "WARNING: Trying to initiate a SIP Connection with unsupported connection type.";
  }
}

void SIPConnection::incomingTCPConnection(std::shared_ptr<TCPConnection> con)
{
  qDebug() << "This SIP connection uses an incoming connection:" << sessionID_;
  if(connection_ != NULL)
  {
    qDebug() << "Replacing existing connection";
  }
  connection_ = con;

  signalConnections();
}

void SIPConnection::signalConnections()
{
  Q_ASSERT(connection_);
  QObject::connect(connection_.get(), &TCPConnection::messageAvailable,
                   this, &SIPConnection::networkPackage);

  QObject::connect(connection_.get(), &TCPConnection::socketConnected,
                   this, &SIPConnection::connectionEstablished);
}

void SIPConnection::connectionEstablished(QString localAddress, QString remoteAddress)
{
  emit sipConnectionEstablished(sessionID_,
                                localAddress,
                                remoteAddress);
}

void SIPConnection::destroyConnection()
{
  if(connection_ == NULL)
  {
    qDebug() << "Trying to destroy an already destroyed connection";
    return;
  }
  connection_->exit(0); // stops qthread
  connection_->stopConnection(); // exits run loop
  while(connection_->isRunning())
  {
    qSleep(5);
  }
  connection_.reset();
}

void SIPConnection::sendRequest(SIPRequest& request, std::shared_ptr<SDPMessageInfo> sdp)
{
  qDebug() << "Composing SIP Request:" << requestToString(request.type);

  QList<SIPField> fields;

  if(!includeViaFields(fields, request.message) ||
     !includeMaxForwardsField(fields, request.message) ||
     !includeToField(fields, request.message) ||
     !includeFromField(fields, request.message) ||
     !includeCallIDField(fields, request.message) ||
     !includeCSeqField(fields, request.message) ||
     !includeContactField(fields, request.message))
  {
    qDebug() << "WARNING: Failed to add all the fields. Probably because of missing values";
    return;
  }
  QString sdp_str = "";
  if(request.type == INVITE)
  {
    if(sdp == NULL)
    {
      qDebug() << "WARNING: SDP missing from INVITE sending";
      return;
    }

    sdp_str = SDPtoString(sdp);

    if(!includeContentLengthField(fields, sdp_str.length()) ||
       !includeContentTypeField(fields, "application/sdp"))
    {
      qDebug() << "WARNING: Could not add sdp fields to request";
      return;
    }
  }
  else
  {
    if(!includeContentLengthField(fields, 0))
    {
      qDebug() << "WARNING: SDP missing from INVITE sending";
      return;
    }
  }

  QString lineEnding = "\r\n";
  QString message = "";
  if(!getFirstRequestLine(message, request))
  {
    qDebug() << "WARNING: could not get first request line";
    return;
  }
  message += lineEnding;

  for(SIPField field : fields)
  {
    message += field.name + ": " + field.values;
    if(field.parameters != NULL)
    {
      for(SIPParameter parameter : *field.parameters)
      {
        message += ";" + parameter.name + "=" + parameter.value;
      }
    }
    message += lineEnding;
  }
  message += lineEnding;
  message += sdp_str;

  if(connection_ != NULL)
  {
    connection_->sendPacket(message);
  }
  else
  {
    qWarning() << "WARNING: Trying to send a packet without connection";
  }
}

void SIPConnection::sendResponse(SIPResponse &response, std::shared_ptr<SDPMessageInfo> sdp)
{
  messageID id = messageComposer_.startSIPResponse(response.type);
  composeHelper(id, response.message);

  if(response.message->transactionRequest == INVITE && response.type == SIP_OK)
  {
    Q_ASSERT(sdp);
    QString sdp_str = messageComposer_.formSDP(sdp);
    messageComposer_.addSDP(id,sdp_str);
  }

  QString message = messageComposer_.composeMessage(id);

  if(connection_ != NULL)
  {
    connection_->sendPacket(message);
  }
  else
  {
    qWarning() << "WARNING: Trying to send a packet without connection";
  }
}

void SIPConnection::composeHelper(uint32_t id, std::shared_ptr<SIPMessageInfo> message)
{
  std::shared_ptr<SIPRoutingInfo> routing = message->routing;
  std::shared_ptr<SIPSessionInfo> session = message->session;

  messageComposer_.to(id, routing->to.realname, routing->to.username,
                        routing->to.host, session->toTag);
  messageComposer_.fromIP(id, routing->from.realname, routing->from.username,
                          QHostAddress(routing->from.host), session->fromTag);

  for(ViaInfo viaAddress : routing->senderReplyAddress)
  {
    QString branch = routing->senderReplyAddress.at(0).branch;
    messageComposer_.viaIP(id, QHostAddress(viaAddress.address), branch);
  }

  messageComposer_.maxForwards(id, routing->maxForwards);
  messageComposer_.setCallID(id, session->callID, routing->from.host);
  messageComposer_.sequenceNum(id, message->cSeq, message->transactionRequest);
}

void SIPConnection::networkPackage(QString message)
{
  qDebug() << "Received a network package for SIP Connection";
  // parse to header and body
  QString header = "";
  QString body = "";
  parsePackage(message, header, body);

  std::shared_ptr<QList<SIPField>> fields;
  std::shared_ptr<QStringList> firstLine;

  if(header != "")
  {
    if(!parseSIPHeader(header))
    {
      qDebug() << "The received message was not correct. ";
      emit parsingError(SIP_BAD_REQUEST, sessionID_); // TODO support other possible error types
      return;
    }
  }
  else
  {
    qDebug() << "The whole message was not received";
  }
}


void SIPConnection::parsePackage(QString package, QString& header, QString& body)
{
  qDebug() << "Parsing package to header and body.";

  if(partialMessage_.length() > 0)
  {
    package = partialMessage_ + package;
    partialMessage_ = "";
  }

  int headerEndIndex = package.indexOf("\r\n\r\n", 0, Qt::CaseInsensitive) + 4;
  int contentLengthIndex = package.indexOf("content-length", 0, Qt::CaseInsensitive);

  qDebug() << "header end at:" << headerEndIndex
           << "and content-length at:" << contentLengthIndex;

  if(contentLengthIndex != -1 && headerEndIndex != -1)
  {
    int contentLengthLineEndIndex = package.indexOf("\r\n", contentLengthIndex, Qt::CaseInsensitive);

    QString value = package.mid(contentLengthIndex + 16, contentLengthLineEndIndex - (contentLengthIndex + 16));

    int valueInt= value.toInt();

    qDebug() << "Content-length:" <<  valueInt;

    if(package.length() < headerEndIndex + valueInt)
    {
      partialMessage_ = package;
    }
    else
    {
      partialMessage_ = package.right(package.length() - (headerEndIndex + valueInt));
      header = package.left(headerEndIndex);
      body = package.mid(headerEndIndex, valueInt);

      qDebug() << "Whole message received.";
      qDebug() << "Header:" << header;
      qDebug() << "Content:" << body;
      qDebug() << "Left overs:" << partialMessage_;
    }
  }
  else
  {
    qDebug() << "Message was not received fully";
    partialMessage_.append(package);
  }
}

bool SIPConnection::parseSIPHeader(QString header)
{
  // Divide into lines
  QStringList lines = header.split("\r\n", QString::SkipEmptyParts);
  qDebug() << "Parsing SIP header with" << lines.size() << "lines";
  if(lines.size() == 0)
  {
    qDebug() << "No first line present in SIP header!";
    return false;
  }

  // parse lines to fields.
  QList<SIPField> fields;
  for(unsigned int i = 1; i < lines.size(); ++i)
  {
    QStringList parameters = lines.at(i).split(";", QString::SkipEmptyParts);

    QRegularExpression re_field("(\\S*): (.+)");
    QRegularExpressionMatch field_match = re_field.match(parameters.at(0));

    if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
    {
      SIPField field = {field_match.captured(1),field_match.captured(2),NULL};
      qDebug() << "Found field: " << field.name;
      if(parameters.size() > 1)
      {
        for(unsigned int j = 1; j < parameters.size(); ++j)
        {
          SIPParameter parameter;
          if(parseParameter(parameters[j], parameter))
          {
            if(field.parameters == NULL)
            {
              field.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
            }
            field.parameters->append(parameter);
          }
          else
          {
            qDebug() << "Failed to parse SIP parameter:" << parameters[j];
          }
        }
      }
      fields.append(field);
    }
    else
    {
      qDebug() << "Failed to parse line:" << lines.at(i) << "Matches:" << field_match.lastCapturedIndex();
    }
  }

  // check that all required header lines are present
  if(!isLinePresent("To", fields)
     || !isLinePresent("From", fields)
     || !isLinePresent("CSeq", fields)
     || (!isLinePresent("Call-ID", fields) && !isLinePresent("i", fields))
     | !isLinePresent("Via", fields))
  {
    qDebug() << "All mandatory header lines not present!";
    return false;
  }

  std::shared_ptr<SIPMessageInfo> message = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo);
  message->cSeq = 0;
  message->transactionRequest = SIP_UNKNOWN_REQUEST;
  message->routing = std::shared_ptr<SIPRoutingInfo> (new SIPRoutingInfo);
  message->routing->maxForwards = 0;
  message->session = std::shared_ptr<SIPSessionInfo> (new SIPSessionInfo);

  for(unsigned int i = 0; i < fields.size(); ++i)
  {
    if(parsing.find(fields[i].name) == parsing.end())
    {
      qDebug() << "Field not supported:" << fields[i].name;
    }
    else
    {
      if(!parsing.at(fields.at(i).name)(fields[i], message))
      {
        qDebug() << "Failed to parse following field:" << fields.at(i).name;
      }
    }
  }

  QRegularExpression re_firstLine("(^(\\w+)|(SIP\/2\.0)) (\\S+) (.*)");
  QRegularExpressionMatch firstline_match = re_firstLine.match(lines[0]);

  if(firstline_match.hasMatch() && firstline_match.lastCapturedIndex() >= 5)
  {
    if(firstline_match.captured(5) == "SIP/2.0")
    {
      qDebug() << "Request detected:" << firstline_match.captured(1);

      message->version = firstline_match.captured(5);
      RequestType requestType = stringToRequest(firstline_match.captured(1));
      if(requestType == SIP_UNKNOWN_REQUEST)
      {
        qDebug() << "Could not recognize request type!";
        return false;
      }

      if(!isLinePresent("Max-Forwards", fields))
      {
        qDebug() << "Mandatory Max-Forwards not present in Request header";
        return false;
      }

      if(requestType == INVITE && !isLinePresent("Contact", fields))
      {
        qDebug() << "Contact header missing from INVITE request";
        return false;
      }

      // construct request

      SIPRequest request;
      request.type = requestType;
      request.message = message;

      emit incomingSIPRequest(request, sessionID_);
    }
    else if(firstline_match.captured(1) == "SIP/2.0")
    {
      qDebug() << "Response detected:" << firstline_match.captured(1);

      ResponseType type = codeToResponse(stringToResponseCode(firstline_match.captured(1)));
      qDebug() << "WARNING: Response parsing is incomplete";

    }
    else
    {
      qDebug() << "Could not identify request or response from:" << lines[0]
               << "with first match as:" << firstline_match.captured(1);
    }
  }
  else
  {
    qDebug() << "Failed to parse first line of SIP message:" << lines[0]
             << "Matches detected:" << firstline_match.lastCapturedIndex();
    return false;
  }

  return true;
}


void SIPConnection::parseSIPaddress(QString address, QString& user, QString& location)
{
  QStringList splitAddress = address.split("@");

  if(splitAddress.size() != 2)
  {
    user = "";
    location = "";
    return;
  }

  user = splitAddress.at(0).right(splitAddress.at(0).length() - 5);
  location = splitAddress.at(1).left(splitAddress.at(1).length() - 1);
}


QList<QHostAddress> SIPConnection::parseIPAddress(QString address)
{
  QList<QHostAddress> ipAddresses;

  QRegularExpression re("\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\.|$)){4}\\b");
  if(re.match(address).hasMatch())
  {
    qDebug() << "Found IPv4 address:" << address;
    ipAddresses.append(QHostAddress(address));
  }
  else
  {
    qDebug() << "Did not find IPv4 address:" << address;
    QHostInfo hostInfo;
    hostInfo.setHostName(address);

    ipAddresses.append(hostInfo.addresses());
  }
  return ipAddresses;
}


QString SIPConnection::SDPtoString(const std::shared_ptr<SDPMessageInfo> sdpInfo)
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

