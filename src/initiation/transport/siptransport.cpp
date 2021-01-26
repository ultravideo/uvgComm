#include "siptransport.h"

#include "siptransporthelper.h"
#include "sipmessagesanity.h"
#include "sipconversions.h"
#include "sipfieldcomposing.h"

#include "statisticsinterface.h"
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


SIPTransport::SIPTransport(quint32 transportID, StatisticsInterface *stats):
  partialMessage_(""),
  connection_(nullptr),
  transportID_(transportID),
  stats_(stats),
  routing_(nullptr),
  processingInProgress_(0)
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

QString SIPTransport::getLocalAddress()
{
  Q_ASSERT(connection_);

  QString address = connection_->localAddress().toString();
  if (connection_->localAddress().protocol() == QAbstractSocket::IPv6Protocol)
  {
    address = "[" + address + "]";
  }

  return address;
}

QString SIPTransport::getRemoteAddress()
{
  Q_ASSERT(connection_);

  QString address = connection_->remoteAddress().toString();
  if (connection_->remoteAddress().protocol() == QAbstractSocket::IPv6Protocol)
  {
    address = "[" + address + "]";
  }

  return address;
}

uint16_t SIPTransport::getLocalPort()
{
  Q_ASSERT(connection_);
  return connection_->localPort();
}


void SIPTransport::createConnection(SIPTransportProtocol type, QString target)
{
  if(type == TCP)
  {
    printNormal(this, "Initiating TCP connection for sip connection",
                {"TransportID"}, QString::number(transportID_));
    connection_ = std::shared_ptr<TCPConnection>(new TCPConnection());
    routing_ = std::shared_ptr<SIPRouting> (new SIPRouting(connection_));
    signalConnections();
    connection_->establishConnection(target, SIP_PORT);
  }
  else
  {
    qDebug() << "WARNING: Trying to initiate a SIP Connection with "
                "unsupported connection type.";
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
  routing_ = std::shared_ptr<SIPRouting> (new SIPRouting(connection_));

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
    printProgramWarning(this, "Trying to destroy an already destroyed connection");
    return;
  }

  if (processingInProgress_ > 0)
  {
     printNormal(this, "Processing in progress while trying to destroy transport");

     while (processingInProgress_ > 0)
     {
       qSleep(5);
     }
  }

  QObject::disconnect(connection_.get(), &TCPConnection::messageAvailable,
                      this, &SIPTransport::networkPackage);

  QObject::disconnect(connection_.get(), &TCPConnection::socketConnected,
                      this, &SIPTransport::connectionEstablished);

  connection_->exit(0); // stops qthread
  connection_->stopConnection(); // exits run loop
  while(connection_->isRunning())
  {
    qSleep(5);
  }

  connection_.reset();

  printNormal(this, "Destroyed SIP Transport connection");
}


void SIPTransport::processOutgoingRequest(SIPRequest& request, QVariant &content)
{
  Q_ASSERT(request.message->contentType == MT_NONE || content.isValid());
  Q_ASSERT(connection_ != nullptr);

  if((request.message->contentType != MT_NONE && !content.isValid()))
  {
    printProgramWarning(this, "Invalid SIP request content when sending");
    return;
  }

  if (connection_ == nullptr)
  {
     printProgramWarning(this, "Connection does not exist in sendRequest");
     return;
  }

  ++processingInProgress_;

  printImportant(this, "Composing and sending SIP Request:", {"Type"},
                 requestMethodToString(request.method));
  if (routing_)
  {
    routing_->processOutgoingRequest(request, content);
  }
  else
  {
    return;
  }

  QString lineEnding = "\r\n";
  QString message = "";

  if(!getFirstRequestLine(message, request, lineEnding))
  {
    printProgramError(this, "Failed to get request first line");
    return;
  }

  // Start composing the request. First we turn the struct to fields
  // which are then turned to string

  // adds content fields and converts the sdp to string if INVITE
  QString content_str = addContent(request.message,
                                   content);

  request.message->authorization.push_back({"joni",
                                            "täällä",
                                            "noncense",
                                            std::shared_ptr<SIP_URI>(new SIP_URI{SIP, {"joni", ""}, {"1.1.1.1", 0}, {}, {}}),
                                            "resp",
                                            SIP_MD5,
                                            "cnoncense",
                                            "op",
                                            SIP_AUTH,
                                            "3"});


  QList<SIPField> fields;
  composeAllFields(fields, request.message);

  // this is just a debug test and can be removed if more performance is desired
  if (!requestSanityCheck(fields, request.method))
  {
    printProgramError(this, "We did not manage to compose a legal SIP request!");
    return;
  }


  message += fieldsToString(fields, lineEnding) + lineEnding;
  message += content_str;

  // print the first line
  stats_->addSentSIPMessage(requestMethodToString(request.method),
                            message,
                            connection_->remoteAddress().toString());

  connection_->sendPacket(message);
  --processingInProgress_;
}


void SIPTransport::processOutgoingResponse(SIPResponse &response, QVariant &content)
{
  ++processingInProgress_;
  printImportant(this, "Composing and sending SIP Response:", {"Type"},
                 responseTypeToPhrase(response.type));
  Q_ASSERT(response.message->cSeq.method != SIP_INVITE
      || response.type != SIP_OK
      || (response.message->contentType == MT_APPLICATION_SDP && content.isValid()));
  Q_ASSERT(connection_ != nullptr);

  if((response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK
     && (!content.isValid() || response.message->contentType != MT_APPLICATION_SDP))
     || connection_ == nullptr)
  {
    printWarning(this, "SDP nullptr or connection does not exist in sendResponse");
    return;
  }

  if (routing_)
  {
    routing_->processOutgoingResponse(response, content);
  }
  else
  {
    return;
  }

  QString lineEnding = "\r\n";
  QString message = "";

  if(!getFirstResponseLine(message, response, lineEnding))
  {
    printProgramError(this, "Failed to compose SIP Response first line!");
    return;
  }

  QString content_str = addContent(response.message, content);

  QList<SIPField> fields;
  composeAllFields(fields, response.message);

  // this is just a debug test and can be removed if more performance is desired
  if (!responseSanityCheck(fields, response.type))
  {
    printProgramError(this, "Failed to compose a correct SIP Response!");
    return;
  }

  message += fieldsToString(fields, lineEnding) + lineEnding;
  message += content_str;

  stats_->addSentSIPMessage(QString::number(responseTypeToCode(response.type))
                            + " " + responseTypeToPhrase(response.type),
                            message,
                            connection_->remoteAddress().toString());


  connection_->sendPacket(message);
  --processingInProgress_;
}



void SIPTransport::networkPackage(QString package)
{
  if (!isConnected())
  {
    printWarning(this, "Connection not open. Discarding received message");
    return;
  }

  ++processingInProgress_;
  // parse to header and body
  QStringList headers;
  QStringList bodies;

  if (!parsePackage(package, headers, bodies) ||  headers.size() != bodies.size())
  {
    printWarning(this, "Did not receive the whole SIP message");
    --processingInProgress_;
    return;
  }

  for (int i = 0; i < headers.size(); ++i)
  {
    QString header = headers.at(i);
    QString body = bodies.at(i);

    QList<SIPField> fields;
    QString firstLine = "";

    if (!headerToFields(header, firstLine, fields))
    {
      printError(this, "Parsing error converting header to fields.");
      return;
    }

    if (header != "" && firstLine != "" && !fields.empty())
    {
      // Here we start identifying is this a request or a response
      QRegularExpression requestRE("^(\\w+) (sip:\\S+@\\S+) SIP/(" + SIP_VERSION + ")");
      QRegularExpression responseRE("^SIP/(" + SIP_VERSION + ") (\\d\\d\\d) (.+)");
      QRegularExpressionMatch request_match = requestRE.match(firstLine);
      QRegularExpressionMatch response_match = responseRE.match(firstLine);

      // Something is wrong if it matches both
      if (request_match.hasMatch() && response_match.hasMatch())
      {
        printDebug(DEBUG_PROGRAM_ERROR, this,
                   "Both the request and response matched, which should not be possible!");
        --processingInProgress_;
        return;
      }

      // If it matches request
      if (request_match.hasMatch() && request_match.lastCapturedIndex() == 3)
      {
        if (isConnected())
        {
          stats_->addReceivedSIPMessage(request_match.captured(1),
                                        package, connection_->remoteAddress().toString());
        }

        if (!parseRequest(request_match.captured(1), request_match.captured(3),
                          fields, body))
        {
          qDebug() << "Failed to parse request";
          emit parsingError(SIP_BAD_REQUEST, transportID_);
        }
      }
      // first line matches a response
      else if (response_match.hasMatch() && response_match.lastCapturedIndex() == 3)
      {
        if (isConnected())
        {
          stats_->addReceivedSIPMessage(response_match.captured(2) + " " + response_match.captured(3),
                                        package, connection_->remoteAddress().toString());
        }

        if (!parseResponse(response_match.captured(2),
                           response_match.captured(1),
                           response_match.captured(3),
                           fields, body))
        {
          qDebug() << "ERROR: Failed to parse response: " << response_match.captured(2);
          emit parsingError(SIP_BAD_REQUEST, transportID_);
        }
      }
      else
      {
        qDebug() << "Failed to parse first line of SIP message:" << firstLine
                 << "Request index:" << request_match.lastCapturedIndex()
                 << "response index:" << response_match.lastCapturedIndex();

        emit parsingError(SIP_BAD_REQUEST, transportID_);
      }
    }
    else
    {
      qDebug() << "The whole message was not received";
    }
  }
  --processingInProgress_;
}


bool SIPTransport::parsePackage(QString package, QStringList& headers, QStringList& bodies)
{
  // get any parts which have been received before
  if(partialMessage_.length() > 0)
  {
    package = partialMessage_ + package;
    partialMessage_ = "";
  }

  int headerEndIndex = package.indexOf("\r\n\r\n", 0, Qt::CaseInsensitive) + 4;
  int contentLengthIndex = package.indexOf("content-length", 0, Qt::CaseInsensitive);

  // read maximum of 20 messages. 3 is -1 + 4
  for (int i = 0; i < 20 && headerEndIndex != 3; ++i)
  {
    printDebug(DEBUG_NORMAL, this,  "Parsing package to header and body",
                {"Messages parsed so far", "Header end index", "content-length index"},
    {QString::number(i), QString::number(headerEndIndex), QString::number(contentLengthIndex)});

    int contentLength = 0;
    // did we find the contact-length field and is it before end of header.
    if (contentLengthIndex != -1 && contentLengthIndex < headerEndIndex)
    {
      int contentLengthLineEndIndex = package.indexOf("\r\n", contentLengthIndex,
                                                      Qt::CaseInsensitive);
      QString value = package.mid(contentLengthIndex + 16,
                                  contentLengthLineEndIndex - (contentLengthIndex + 16));

      contentLength = value.toInt();
      if (contentLength < 0)
      {
        // TODO: Warn the user maybe. Maybe also ban the user at least temporarily.
        printDebug(DEBUG_PEER_ERROR, this,
                   "Got negative content-length! Peer is doing something very strange.");
        return false;
      }
    }

    printNormal(this, "Parsed Content-length", {"Content-length"}, {QString::number(contentLength)});

    // if we have the whole message
    if(package.length() >= headerEndIndex + contentLength)
    {
      headers.push_back(package.left(headerEndIndex));
      bodies.push_back(package.mid(headerEndIndex, contentLength));

      //printDebug(DEBUG_IMPORTANT,this, "Whole SIP message Received",
      //            {"Body", "Content"}, {headers.last(), bodies.last()});

      package = package.right(package.length() - (headerEndIndex + contentLength));
    }

    headerEndIndex = package.indexOf("\r\n\r\n", 0, Qt::CaseInsensitive) + 4;
    contentLengthIndex = package.indexOf("content-length", 0, Qt::CaseInsensitive);
  }

  partialMessage_ = package;
  return !headers.empty() && headers.size() == bodies.size();
}


bool SIPTransport::parseRequest(QString requestString, QString version,
                                QList<SIPField> &fields, QString& body)
{  
  qDebug() << "Request detected:" << requestString;

  printImportant(this, "Parsing incoming request", {"Type"}, {requestString});
  SIPRequest request;
  request.method = stringToRequestMethod(requestString);
  request.sipVersion = version;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);


  if(request.method == SIP_NO_REQUEST)
  {
    qDebug() << "Could not recognize request type!";
    return false;
  }

  if (!requestSanityCheck(fields, request.method))
  {
    return false;
  }

  if (!fieldsToMessageHeader(fields, request.message))
  {
    return false;
  }

  QVariant content;
  if (body != "" && request.message->contentType != MT_NONE)
  {
    parseContent(content, request.message->contentType, body);
  }

  emit incomingRequest(request, getLocalAddress(), content, transportID_);
  return true;
}


bool SIPTransport::parseResponse(QString responseString, QString version,
                                 QString text, QList<SIPField> &fields,
                                 QString& body)
{
  printImportant(this, "Parsing incoming response", {"Type"}, {responseString});

  SIPResponse response;
  response.type = codeToResponseType(stringToResponseCode(responseString));
  response.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
  response.text = text;
  response.sipVersion = version;

  if (!responseSanityCheck(fields, response.type))
  {
    return false;
  }

  if (!fieldsToMessageHeader(fields, response.message))
  {
    return false;
  }

  QVariant content;
  if (body != "" && response.message->contentType != MT_NONE)
  {
    parseContent(content, response.message->contentType, body);
  }

  if (routing_)
  {
    routing_->processIncomingResponse(response, content);
  }
  else
  {
    return false;
  }

  emit incomingResponse(response, content);

  return true;
}


