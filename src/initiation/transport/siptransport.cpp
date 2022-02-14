#include "siptransport.h"

#include "siptransporthelper.h"
#include "sipmessagesanity.h"
#include "sipconversions.h"
#include "sipfieldcomposing.h"

#include "statisticsinterface.h"

#include "common.h"
#include "logger.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include <QList>
#include <QHostInfo>

#include <iostream>
#include <sstream>
#include <string>

#include <functional>


SIPTransport::SIPTransport(StatisticsInterface *stats):
  partialMessage_(""),
  stats_(stats),
  processingInProgress_(0)
{}

SIPTransport::~SIPTransport()
{}



void SIPTransport::processOutgoingRequest(SIPRequest& request, QVariant &content)
{
  Q_ASSERT(request.message->contentType == MT_NONE || content.isValid());

  if((request.message->contentType != MT_NONE && !content.isValid()))
  {
    Logger::getLogger()->printProgramWarning(this, "Invalid SIP request content when sending");
    return;
  }

  ++processingInProgress_;

  Logger::getLogger()->printNormal(this, "Processing outgoing request:", {"Type"},
                 requestMethodToString(request.method));

  QString lineEnding = "\r\n";
  QString firstLine = "";

  if(!getFirstRequestLine(firstLine, request, lineEnding))
  {
    Logger::getLogger()->printProgramError(this, "Failed to get request first line");
    return;
  }

  // Start composing the request. First we turn the struct to fields
  // which are then turned to string

  // adds content fields and converts the sdp to string if INVITE
  QString content_str = addContent(request.message,
                                   content);

  request.message->userAgent = "Kvazzup";


  QList<SIPField> fields;
  composeAllFields(fields, request.message);

  // this is just a debug test and can be removed if more performance is desired
  if (!requestSanityCheck(fields, request.method))
  {
    Logger::getLogger()->printProgramError(this, "We did not manage to compose a legal SIP request!");
    return;
  }

  QString header = fieldsToString(fields, lineEnding);
  QString message = firstLine + header + lineEnding + content_str;

  // add the message to statistics
  stats_->addSentSIPMessage(requestMethodToString(request.method),
                            firstLine + header,
                            contentTypeToString(request.message->contentType),
                            content_str);

  emit sendMessage(message);
  --processingInProgress_;
}


void SIPTransport::processOutgoingResponse(SIPResponse &response, QVariant &content)
{
  ++processingInProgress_;
  Logger::getLogger()->printNormal(this, "Processing outgoing response:", {"Type"},
                                   responseTypeToPhrase(response.type));
  Q_ASSERT(response.message->cSeq.method != SIP_INVITE
      || response.type != SIP_OK
      || (response.message->contentType == MT_APPLICATION_SDP && content.isValid()));

  if(response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK
     && (!content.isValid() || response.message->contentType != MT_APPLICATION_SDP))
  {
    Logger::getLogger()->printWarning(this, "SDP nullptr in sendResponse");
    return;
  }

  QString lineEnding = "\r\n";
  QString firstLine = "";

  if(!getFirstResponseLine(firstLine, response, lineEnding))
  {
    Logger::getLogger()->printProgramError(this, "Failed to compose SIP Response first line!");
    return;
  }

  QString content_str = addContent(response.message, content);

  QList<SIPField> fields;
  composeAllFields(fields, response.message);

  // this is just a debug test and can be removed if more performance is desired
  if (!responseSanityCheck(fields, response.type))
  {
    Logger::getLogger()->printProgramError(this, "Failed to compose a correct SIP Response!");
    return;
  }

  QString header = fieldsToString(fields, lineEnding);
  QString message = firstLine + header + lineEnding + content_str;

  // add response to statistics
  stats_->addSentSIPMessage(QString::number(responseTypeToCode(response.type))
                            + " " + responseTypeToPhrase(response.type),
                            firstLine + header,
                            contentTypeToString(response.message->contentType),
                            content_str);

  emit sendMessage(message);
  --processingInProgress_;
}



void SIPTransport::networkPackage(QString package)
{
  Logger::getLogger()->printNormal(this, "Parsing incoming network package");

  ++processingInProgress_;
  // parse to header and body
  QStringList headers;
  QStringList bodies;

  if (!parsePackage(package, headers, bodies) ||  headers.size() != bodies.size())
  {
    Logger::getLogger()->printNormal(this, "Did not receive the whole SIP message");
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
      Logger::getLogger()->printError(this, "Parsing error converting header to fields.");
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
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                   "Both the request and response matched, which should not be possible!");
        --processingInProgress_;
        return;
      }

      // If it matches request
      if (request_match.hasMatch() && request_match.lastCapturedIndex() == 3)
      {
        if (!parseRequest(header, request_match.captured(1), request_match.captured(3),
                          fields, body))
        {
          Logger::getLogger()->printWarning(this, "Failed to parse request");
          //emit parsingError(SIP_BAD_REQUEST, getRemoteAddress());
        }
      }
      // first line matches a response
      else if (response_match.hasMatch() && response_match.lastCapturedIndex() == 3)
      {

        if (!parseResponse(header, response_match.captured(2),
                           response_match.captured(1),
                           response_match.captured(3),
                           fields, body))
        {
          Logger::getLogger()->printWarning(this, "Failed to parse response", "Type",
                                            response_match.captured(2));
          //emit parsingError(SIP_BAD_REQUEST, getRemoteAddress());
        }
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Failed to parse first line of SIP message",
                                          {"First Line", "RequestIndex", "ResponseIndex"},
                                          {firstLine,
                                           QString::number(request_match.lastCapturedIndex()),
                                           QString::number(response_match.lastCapturedIndex())});

        //emit parsingError(SIP_BAD_REQUEST, connection_->remoteAddress());
      }
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Received partial SIP message");
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

  // read maximum of 20 SIP messages. 3 is -1 + 4
  for (int i = 0; i < 20 && headerEndIndex != 3; ++i)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "Parsing package to header and body",
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
        // TODO: Warn the user maybe. Maybe also ban the peer at least temporarily.
        Logger::getLogger()->printDebug(DEBUG_PEER_ERROR, this,
                   "Got negative content-length! Peer is doing something very strange.");
        return false;
      }
    }

    Logger::getLogger()->printNormal(this, "Parsed Content-length", {"Content-length"}, {QString::number(contentLength)});

    // if we have the whole message
    if(package.length() >= headerEndIndex + contentLength)
    {
      headers.push_back(package.left(headerEndIndex));
      bodies.push_back(package.mid(headerEndIndex, contentLength));

      //Logger::getLogger()->printDebug(DEBUG_IMPORTANT,this, "Whole SIP message Received",
      //            {"Body", "Content"}, {headers.last(), bodies.last()});

      package = package.right(package.length() - (headerEndIndex + contentLength));
    }

    headerEndIndex = package.indexOf("\r\n\r\n", 0, Qt::CaseInsensitive) + 4;
    contentLengthIndex = package.indexOf("content-length", 0, Qt::CaseInsensitive);
  }

  partialMessage_ = package;
  return !headers.empty() && headers.size() == bodies.size();
}


bool SIPTransport::parseRequest(const QString &header, QString requestString, QString version,
                                QList<SIPField> &fields, QString& body)
{  
  Logger::getLogger()->printImportant(this, "Parsing incoming request", {"Type"}, {requestString});

  SIPRequest request;
  request.method = stringToRequestMethod(requestString);
  request.sipVersion = version;
  request.message = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);


  if(request.method == SIP_NO_REQUEST)
  {
    Logger::getLogger()->printWarning(this, "Could not recognize request type!");
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

  stats_->addReceivedSIPMessage(requestString, header,
                                contentTypeToString(request.message->contentType), body);

  emit incomingRequest(request, content, SIP_NO_RESPONSE);
  return true;
}


bool SIPTransport::parseResponse(const QString &header, QString responseString,
                                 QString version, QString text, QList<SIPField> &fields,
                                 QString& body)
{
  Logger::getLogger()->printImportant(this, "Parsing incoming response", {"Type"}, {responseString});

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

  stats_->addReceivedSIPMessage(QString::number(responseTypeToCode(response.type))
                                + " " + response.text, header,
                                contentTypeToString(response.message->contentType), body);

  emit incomingResponse(response, content, false);

  return true;
}


