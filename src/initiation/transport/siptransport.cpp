#include "siptransport.h"

#include "sipconversions.h"
#include "sipfieldparsing.h"
#include "sipfieldcomposing.h"
#include "initiation/negotiation/sipcontent.h"
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


// TODO: separate this into common, request and response field parsing.
// This is so we can ignore nonrelevant fields (7.3.2)

// RFC3261_TODO: support compact forms (7.3.3)

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


SIPTransport::SIPTransport(quint32 transportID):
  partialMessage_(""),
  connection_(nullptr),
  transportID_(transportID)
{}

SIPTransport::~SIPTransport()
{}

void SIPTransport::cleanup()
{
  destroyConnection();
}

bool SIPTransport::isConnected()
{
  return connection_ && connection_->isConnected();
}

QHostAddress SIPTransport::getLocalAddress()
{
  Q_ASSERT(connection_);
  return connection_->localAddress();
}

QHostAddress SIPTransport::getRemoteAddress()
{
  Q_ASSERT(connection_);
  return connection_->remoteAddress();
}

void SIPTransport::createConnection(ConnectionType type, QString target)
{
  if(type == TCP)
  {
    qDebug() << "Connecting, SIP Transport: Initiating TCP connection for sip connection number:" << transportID_;
    connection_ = std::shared_ptr<TCPConnection>(new TCPConnection);
    signalConnections();
    connection_->establishConnection(target, SIP_PORT);
  }
  else
  {
    qDebug() << "WARNING: Trying to initiate a SIP Connection with unsupported connection type.";
  }
}

void SIPTransport::incomingTCPConnection(std::shared_ptr<TCPConnection> con)
{
  qDebug() << "This SIP connection uses an incoming connection:" << transportID_;
  if(connection_ != nullptr)
  {
    qDebug() << "Replacing existing connection";
  }
  connection_ = con;

  signalConnections();
}

void SIPTransport::signalConnections()
{
  Q_ASSERT(connection_);
  QObject::connect(connection_.get(), &TCPConnection::messageAvailable,
                   this, &SIPTransport::networkPackage);

  QObject::connect(connection_.get(), &TCPConnection::socketConnected,
                   this, &SIPTransport::connectionEstablished);
}

void SIPTransport::connectionEstablished(QString localAddress, QString remoteAddress)
{
  emit sipTransportEstablished(transportID_,
                                localAddress,
                                remoteAddress);
}

void SIPTransport::destroyConnection()
{
  if(connection_ == nullptr)
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

void SIPTransport::sendRequest(SIPRequest& request, QVariant &content)
{
  qDebug() << "Composing SIP Request:" << requestToString(request.type);
  Q_ASSERT((request.type != SIP_INVITE) ||
      (request.message->content.type == APPLICATION_SDP && content.isValid()));
  Q_ASSERT(connection_ != nullptr);

  if(((request.type == SIP_INVITE) &&
     (request.message->content.type != APPLICATION_SDP || !content.isValid()))
     || connection_ == nullptr)
  {
    qDebug() << "WARNING: SDP nullptr or connection does not exist in sendRequest";
    return;
  }

  QList<SIPField> fields;
  if(!composeMandatoryFields(fields, request.message) ||
     !includeContactField(fields, request.message))
  {
    qDebug() << "WARNING: Failed to add all the fields. Probably because of missing values.";
    return;
  }

  QString lineEnding = "\r\n";
  QString message = "";
  // adds content fields and converts the sdp to string if INVITE
  QString sdp_str = addContent(fields, request.type == SIP_INVITE, content.value<SDPMessageInfo>());
  if(!getFirstRequestLine(message, request, lineEnding))
  {
    qDebug() << "WARNING: could not get first request line";
    return;
  }
  message += fieldsToString(fields, lineEnding) + lineEnding;
  message += sdp_str;
  connection_->sendPacket(message);
}

void SIPTransport::sendResponse(SIPResponse &response, QVariant &content)
{
  qDebug() << "Composing SIP Response:" << responseToPhrase(response.type);
  Q_ASSERT(response.message->transactionRequest != SIP_INVITE
           || response.type != SIP_OK || (response.message->content.type == APPLICATION_SDP && content.isValid()));
  Q_ASSERT(connection_ != nullptr);

  if((response.message->transactionRequest == SIP_INVITE && response.type == SIP_OK
     && (!content.isValid() || response.message->content.type != APPLICATION_SDP))
     || connection_ == nullptr)
  {
    qDebug() << "WARNING: SDP nullptr or connection does not exist in sendResponse";
    return;
  }

  QList<SIPField> fields;
  if(!composeMandatoryFields(fields, response.message))
  {
    qDebug() << "WARNING: Failed to add mandatory fields. Probably because of missing values.";
    return;
  }

  // TODO: if the response is 405 SIP_NOT_ALLOWED we must include an allow header field.
  // lists allowed methods.

  QString lineEnding = "\r\n";
  QString message = "";
  // adds content fields and converts the sdp to string if INVITE
  QString sdp_str = addContent(fields, response.message->transactionRequest == SIP_INVITE
                               && response.type == SIP_OK, content.value<SDPMessageInfo>());
  if(!getFirstResponseLine(message, response, lineEnding))
  {
    qDebug() << "WARNING: could not get first request line";
    return;
  }
  message += fieldsToString(fields, lineEnding) + lineEnding;
  message += sdp_str;
  connection_->sendPacket(message);
}

bool SIPTransport::composeMandatoryFields(QList<SIPField>& fields, std::shared_ptr<SIPMessageInfo> message)
{
  return includeViaFields(fields, message) &&
         includeMaxForwardsField(fields, message) &&
         includeToField(fields, message) &&
         includeFromField(fields, message) &&
         includeCallIDField(fields, message) &&
         includeCSeqField(fields, message);
}

QString SIPTransport::fieldsToString(QList<SIPField>& fields, QString lineEnding)
{
  QString message = "";
  for(SIPField field : fields)
  {
    message += field.name + ": " + field.values;
    if(field.parameters != nullptr)
    {
      for(SIPParameter parameter : *field.parameters)
      {
        message += ";" + parameter.name + "=" + parameter.value;
      }
    }
    message += lineEnding;
  }
  return message;
}


void SIPTransport::networkPackage(QString package)
{
  qDebug() << "Received a network package for SIP Connection";
  // parse to header and body
  QString header = "";
  QString body = "";
  parsePackage(package, header, body);

  QList<SIPField> fields;
  QString firstLine = "";

  headerToFields(header, firstLine, fields);

  if(header != "" && firstLine != "" && !fields.empty())
  {
    std::shared_ptr<SIPMessageInfo> message;
    if(!fieldsToMessage(fields, message))
    {
      qDebug() << "The received message was not correct. ";
      emit parsingError(SIP_BAD_REQUEST, transportID_); // RFC3261_TODO support other possible error types
      return;
    }

    QVariant content;
    if(body != "" && message->content.type != NO_CONTENT)
    {
      parseContent(content, message->content.type, body);
    }

    QRegularExpression requestRE("^(\\w+) (sip:\\S+@\\S+) (SIP\/2\.0)");
    QRegularExpression responseRE("^(SIP\/2\.0) (\\d\\d\\d) (\\w| )+");
    QRegularExpressionMatch request_match = requestRE.match(firstLine);
    QRegularExpressionMatch response_match = responseRE.match(firstLine);

    if(request_match.hasMatch() && response_match.hasMatch())
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_RECEIVE_SIP,
                 "Both the request and response matched, which should not be possible!");
      return;
    }

    if(request_match.hasMatch() && request_match.lastCapturedIndex() == 3)
    {
      if(!parseRequest(request_match.captured(1), request_match.captured(3), message, fields, content))
      {
        qDebug() << "Failed to parse request";
      }
    }
    else if(response_match.hasMatch() && response_match.lastCapturedIndex() == 3)
    {
      if(!parseResponse(response_match.captured(2), response_match.captured(1), message, content))
      {
        qDebug() << "Failed to parse response: " << response_match.captured(2);
      }
    }
    else
    {
      qDebug() << "Failed to parse first line of SIP message:" << firstLine
               << "Request index:" << request_match.lastCapturedIndex()
               << "response index:" << response_match.lastCapturedIndex();
    }
  }
  else
  {
    qDebug() << "The whole message was not received";
  }
}


void SIPTransport::parsePackage(QString package, QString& header, QString& body)
{
  qDebug() << "Parsing package to header and body.";

  // RFC3261_TODO: make this work also without content length.
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

      qDebug().noquote() << "\r\n";
      qDebug().noquote() << "Whole SIP message received ----------- " << "\r\n";
      qDebug().noquote() << "Header:" << header;
      qDebug().noquote() << "Content:" << body;
      qDebug().noquote() << "Left overs:" << partialMessage_ << "\r\n";
    }
  }
  else
  {
    qDebug() << "Message was not received fully";
    partialMessage_.append(package);
  }
}

bool SIPTransport::headerToFields(QString header, QString& firstLine, QList<SIPField>& fields)
{
  // RFC3261_TODO: support header fields that span multiple lines
  // Divide into lines
  QStringList lines = header.split("\r\n", QString::SkipEmptyParts);
  qDebug() << "Parsing SIP header with" << lines.size() << "lines";
  if(lines.size() == 0)
  {
    qDebug() << "No first line present in SIP header!";
    return false;
  }
  firstLine = lines.at(0);

  // RFC3261_TODO: Support comma(,) separated header fields. (expect for some field types)

  // parse lines to fields.
  QStringList debugLineNames = {};
  for(int i = 1; i < lines.size(); ++i)
  {
    QStringList parameters = lines.at(i).split(";", QString::SkipEmptyParts);

    // RFC3261_TODO: support input with unknown amounts of empty spaces
    QRegularExpression re_field("(\\S*): (.+)");
    QRegularExpressionMatch field_match = re_field.match(parameters.at(0));


    if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
    {
      // RFC3261_TODO: Uniformalize case formatting. Make everything big or small case expect quotes.

      SIPField field = {field_match.captured(1),field_match.captured(2),nullptr};
      debugLineNames << field.name;
      if(parameters.size() > 1)
      {
        for(int j = 1; j < parameters.size(); ++j)
        {
          SIPParameter parameter;
          // TODO: check that parameter does not already exist
          if(parseParameter(parameters[j], parameter))
          {
            if(field.parameters == nullptr)
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

  qDebug() << "Found following SIP fields:" << debugLineNames;

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
  return true;
}


bool SIPTransport::fieldsToMessage(QList<SIPField>& fields,
                                    std::shared_ptr<SIPMessageInfo>& message)
{
  message = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo);
  message->cSeq = 0;
  message->transactionRequest = SIP_NO_REQUEST;
  message->maxForwards = 0;
  message->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo);

  for(int i = 0; i < fields.size(); ++i)
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
        qDebug() << "Values:" << fields.at(i).values;
        return false;
      }
    }
  }
  return true;
}


bool SIPTransport::parseRequest(QString requestString, QString version,
                                 std::shared_ptr<SIPMessageInfo> message,
                                 QList<SIPField> &fields, QVariant &content)
{
  qDebug() << "Request detected:" << requestString;

  message->version = version; // TODO: set only version not SIP/version
  RequestType requestType = stringToRequest(requestString);
  if(requestType == SIP_NO_REQUEST)
  {
    qDebug() << "Could not recognize request type!";
    return false;
  }

  // RFC3261_TODO: if more than one via-field, discard message

  if(!isLinePresent("Max-Forwards", fields))
  {
    qDebug() << "Mandatory Max-Forwards not present in Request header";
    return false;
  }

  if(requestType == SIP_INVITE && !isLinePresent("Contact", fields))
  {
    qDebug() << "Contact header missing from INVITE request";
    return false;
  }

  // construct request
  SIPRequest request;
  request.type = requestType;
  request.message = message;

  emit incomingSIPRequest(request, getLocalAddress(), content, transportID_);
  return true;
}


bool SIPTransport::parseResponse(QString responseString, QString version,
                                  std::shared_ptr<SIPMessageInfo> message, QVariant &content)
{
  qDebug() << "Response detected:" << responseString;
  message->version = version; // TODO: set only version not SIP/version
  ResponseType type = codeToResponse(stringToResponseCode(responseString));

  if(type == SIP_UNKNOWN_RESPONSE)
  {
    qDebug() << "Could not recognize response type!";
    return false;
  }

  SIPResponse response;
  response.type = type;
  response.message = message;

  emit incomingSIPResponse(response, content);

  return true;
}

void SIPTransport::parseContent(QVariant& content, ContentType type, QString& body)
{
  if(type == APPLICATION_SDP)
  {
    SDPMessageInfo sdp;
    if(parseSDPContent(body, sdp))
    {
      qDebug () << "Successfully parsed SDP";
      content.setValue(sdp);
    }
    else
    {
      qDebug () << "Failed to parse SDP message";
    }
  }
  else
  {
    qDebug() << "Unsupported content type detected!";
  }
}

void SIPTransport::parseSIPaddress(QString address, QString& user, QString& location)
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


QList<QHostAddress> SIPTransport::parseIPAddress(QString address)
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


QString SIPTransport::addContent(QList<SIPField>& fields, bool haveContent, const SDPMessageInfo &sdp)
{
  QString sdp_str = "";

  if(haveContent)
  {
    sdp_str = composeSDPContent(sdp);
    if(sdp_str == "" ||
       !includeContentLengthField(fields, sdp_str.length()) ||
       !includeContentTypeField(fields, "application/sdp"))
    {
      qDebug() << "WARNING: Could not add sdp fields to request";
      return "";
    }
  }
  else if(!includeContentLengthField(fields, 0))
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SIP_CONTENT, "Could not add content-length field to sip message!");
  }
  return sdp_str;
}
