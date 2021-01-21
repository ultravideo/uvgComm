#include "sipfieldparsing.h"


#include "sipfieldparsinghelper.h"
#include "sipconversions.h"
#include "common.h"

#include <QRegularExpression>
#include <QDebug>



bool parsingPreChecks(const SIPField &field,
                      std::shared_ptr<SIPMessageHeader> message,
                      bool emptyPossible)
{
  Q_ASSERT(message != nullptr);

  // does message exist and do we have some values if we are expecting those
  if (message == nullptr ||
      (!emptyPossible && field.commaSeparated.empty()))
  {
    printError("SIP Field Parsing", "Parsing prechecks failed");
    return false;
  }

  // Check that none of the values have zero words.
  // Empty fields should be handled with having no values, not with empty word lists
  // this guarantees that there will always be elements in words lists
  for (auto& value: field.commaSeparated)
  {
    Q_ASSERT(!value.words.empty());

    if (value.words.empty())
    {
      printError("SIP Field Parsing", "Found empty word list");
      return false;
    }
  }

  return true;
}



bool parseAcceptField(const SIPField &field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  // in case there are multiple accept fields, we don't want to reset previous
  if (message->accept)
  {
    message->accept = std::shared_ptr<QList<SIPAccept>> (new QList<SIPAccept>());
  }

  for (auto& value : field.commaSeparated)
  {
    if (value.words.size() == 1)
    {
      // parse accepted media type
      MediaType accept = stringToContentType(value.words.first());

      // if we recognize this type
      if (accept != MT_NONE &&
          accept != MT_UNKNOWN)
      {
        // add it to struct
        message->accept->push_back({accept, {}});
        copyParameterList(value.parameters, message->accept->back().parameters);
      }
    }
  }

  return false;
}


bool parseAcceptEncodingField(const SIPField &field,
                              std::shared_ptr<SIPMessageHeader> message)
{
  return parseAcceptGeneric(field, message->acceptEncoding);
}


bool parseAcceptLanguageField(const SIPField& field,
                              std::shared_ptr<SIPMessageHeader> message)
{
  return parseAcceptGeneric(field, message->acceptLanguage);
}


bool parseAlertInfoField(const SIPField &field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return parseInfo(field, message->alertInfos);
}


bool parseAllowField(const SIPField &field,
                     std::shared_ptr<SIPMessageHeader> message)
{
  if (message->allow == nullptr)
  {
    message->allow =
        std::shared_ptr<QList<SIPRequestMethod>> (new QList<SIPRequestMethod>());
  }

  for (auto& value : field.commaSeparated)
  {
    if (value.words.size() == 1)
    {
      SIPRequestMethod method = stringToRequestMethod(value.words[0]);

      if (method != SIP_NO_REQUEST &&
          method != SIP_UNKOWN_REQUEST)
      {
        message->allow->push_back(method);
      }
    }
  }

  return true;
}


bool parseAuthInfoField(const SIPField &field,
                        std::shared_ptr<SIPMessageHeader> message)
{
  message->authInfo = std::shared_ptr<SIPAuthInfo> (new SIPAuthInfo);

  std::map<QString, QString> digests;
  populateDigestTable(field.commaSeparated, digests, false);

  message->authInfo->nextNonce = getDigestTableValue(digests, "nextnonce");
  message->authInfo->messageQop = stringToQopValue(getDigestTableValue(digests,
                                                                       "nextnonce"));
  message->authInfo->responseAuth = getDigestTableValue(digests, "nextnonce");
  message->authInfo->cnonce = getDigestTableValue(digests, "nextnonce");
  message->authInfo->nonceCount = getDigestTableValue(digests, "nextnonce");

  // did we actually get anything succesfully
  if (message->authInfo->nextNonce == "" &&
      message->authInfo->messageQop == SIP_NO_AUTH &&
      message->authInfo->responseAuth == "" &&
      message->authInfo->cnonce == "" &&
      message->authInfo->nonceCount == "")

  {
    message->authInfo = nullptr;
    return false;
  }

  return true;
}


bool parseAuthorizationField(const SIPField &field,
                             std::shared_ptr<SIPMessageHeader> message)
{
  return parseDigestResponseField(field, message->authorization);
}


bool parseCallIDField(const SIPField &field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->callID, false);
}


bool parseCallInfoField(const SIPField &field,
                        std::shared_ptr<SIPMessageHeader> message)
{
  return parseInfo(field, message->callInfos);
}


bool parseContactField(const SIPField &field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return parseSIPRouteList(field, message->contact);
}


bool parseContentDispositionField(const SIPField &field,
                                  std::shared_ptr<SIPMessageHeader> message)
{
  if (field.commaSeparated.first().words.size() != 1)
  {
    return false;
  }

  message->contentDisposition =
      std::shared_ptr<ContentDisposition> (new ContentDisposition);

  message->contentDisposition->dispType = field.commaSeparated.first().words.first();

  copyParameterList(field.commaSeparated.first().parameters,
                    message->contentDisposition->parameters);

  return true;
}


bool parseContentEncodingField(const SIPField &field,
                               std::shared_ptr<SIPMessageHeader> message)
{

  return parseStringList(field, message->contentEncoding);
}


bool parseContentLanguageField(const SIPField &field,
                               std::shared_ptr<SIPMessageHeader> message)
{
  return parseStringList(field, message->contentLanguage);
}


bool parseContentLengthField(const SIPField &field,
                             std::shared_ptr<SIPMessageHeader> message)
{
  return field.commaSeparated[0].words.size() == 1 &&
      parseUint64(field.commaSeparated[0].words[0], message->contentLength);
}


bool parseContentTypeField(const SIPField &field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  message->contentType = stringToContentType(field.commaSeparated[0].words[0]);

  if (message->contentType == MT_UNKNOWN)
  {
    printWarning("SIP Field Parsing", "Detected unknown mediatype");
    return false;
  }

  return true;
}


bool parseCSeqField(const SIPField &field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  if (field.commaSeparated[0].words.size() != 2)
  {
    return false;
  }

  if (!parseUint32(field.commaSeparated[0].words[0], message->cSeq.cSeq))
  {
    return false;
  }
  message->cSeq.method = stringToRequestMethod(field.commaSeparated[0].words[1]);
  if (message->cSeq.method == SIP_NO_REQUEST ||
      message->cSeq.method == SIP_UNKOWN_REQUEST)
  {
    message->cSeq.cSeq = 0;
    return false;
  }

  return true;
}


bool parseDateField(const SIPField &field,
                    std::shared_ptr<SIPMessageHeader> message)
{
  if (field.commaSeparated.size() != 2 ||
      field.commaSeparated.at(0).words.size() != 1 ||
      field.commaSeparated.at(1).words.size() != 5)
  {
    return false;
  }

  message->date = std::shared_ptr<SIPDateField> (new SIPDateField);

  message->date->weekday = field.commaSeparated.at(0).words.at(0);
  bool daySuccess = parseUint8(field.commaSeparated.at(1).words.at(0), message->date->day);
  message->date->month = field.commaSeparated.at(1).words.at(1);
  bool yearSuccess = parseUint32(field.commaSeparated.at(1).words.at(2), message->date->year);
  message->date->time = field.commaSeparated.at(1).words.at(3);
  message->date->timezone = field.commaSeparated.at(1).words.at(4);

  // check most of values for possible errors
  if ((message->date->weekday != "Mon" &&
      message->date->weekday != "Tue" &&
      message->date->weekday != "Wed" &&
      message->date->weekday != "Thu" &&
      message->date->weekday != "Fri" &&
      message->date->weekday != "Sat" &&
      message->date->weekday != "Sun") ||
      !daySuccess ||
      message->date->day < 1 || message->date->day > 31 ||
      (message->date->month != "Jan" &&
      message->date->month != "Feb" &&
      message->date->month != "Mar" &&
      message->date->month != "Apr" &&
      message->date->month != "May" &&
      message->date->month != "Jun" &&
      message->date->month != "Jul" &&
      message->date->month != "Aug" &&
      message->date->month != "Sep" &&
      message->date->month != "Oct" &&
      message->date->month != "Nov" &&
      message->date->month != "Dec") ||
      !yearSuccess ||
      message->date->time.size() != 8 ||
      message->date->timezone != "GMT")
  {
    message->date = nullptr;
    return false;
  }

  return true;
}


bool parseErrorInfoField(const SIPField &field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return parseInfo(field, message->errorInfos);
}


bool parseExpireField(const SIPField &field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  return parseSharedUint32(field, message->expires);
}


bool parseFromField(const SIPField &field,
                    std::shared_ptr<SIPMessageHeader> message)
{
  if (!parseNameAddr(field.commaSeparated[0].words, message->from.address))
  {
    return false;
  }

  // from tag should always be included
  return parseParameterByName(field.commaSeparated[0].parameters,
                              "tag", message->from.tag);
}


bool parseInReplyToField(const SIPField &field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->inReplyToCallID, false);
}


bool parseMaxForwardsField(const SIPField &field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  if (field.commaSeparated[0].words.size() != 1)
  {
    return false;
  }

  uint8_t value = 0;

  if (!parseUint8(field.commaSeparated[0].words[0], value))
  {
    return false;
  }

  field.commaSeparated[0].words[0], message->maxForwards =
      std::shared_ptr<uint8_t> (new uint8_t{value});
  return true;
}


bool parseMinExpiresField(const SIPField &field,
                          std::shared_ptr<SIPMessageHeader> message)
{
  return parseSharedUint32(field, message->minExpires);
}


bool parseMIMEVersionField(const SIPField &field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->mimeVersion, false);
}


bool parseOrganizationField(const SIPField &field,
                            std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->organization, true);
}


bool parsePriorityField(const SIPField &field,
                        std::shared_ptr<SIPMessageHeader> message)
{
  // preconditions should be called, so this should be safe
  message->priority = stringToPriority(field.commaSeparated.at(0).words.at(0));

  if (message->priority != SIP_NO_PRIORITY &&
      message->priority != SIP_UNKNOWN_PRIORITY)
  {
    return false;
  }

  return true;
}


bool parseProxyAuthenticateField(const SIPField &field,
                                 std::shared_ptr<SIPMessageHeader> message)
{
  return parseDigestChallengeField(field, message->proxyAuthenticate);
}


bool parseProxyAuthorizationField(const SIPField& field,
                                  std::shared_ptr<SIPMessageHeader> message)
{
  return parseDigestResponseField(field, message->proxyAuthorization);
}


bool parseProxyRequireField(const SIPField &field,
                            std::shared_ptr<SIPMessageHeader> message)
{
  return parseStringList(field, message->proxyRequires);
}


bool parseRecordRouteField(const SIPField &field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  return parseSIPRouteList(field, message->recordRoutes);
}


bool parseReplyToField(const SIPField &field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  message->replyTo = std::shared_ptr<SIPRouteLocation> (new SIPRouteLocation);

  if (!parseSIPRouteLocation(field.commaSeparated.first(), *message->replyTo))
  {
    message->replyTo = nullptr;
    return false;
  }

  return true;
}


bool parseRequireField(const SIPField &field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return parseStringList(field, message->require);
}


bool parseRetryAfterField(const SIPField &field,
                          std::shared_ptr<SIPMessageHeader> message)
{
  message->retryAfter = std::shared_ptr<SIPRetryAfter> (new SIPRetryAfter);

  if (parseUint32(field.commaSeparated.first().words.first(),
                  message->retryAfter->time))
  {
    message->retryAfter = nullptr;
    return false;
  }

  QString duration = "";
  parseParameterByName(field.commaSeparated.first().parameters,
                       "duration", duration);

  if (duration != "")
  {
    parseUint32(duration, message->retryAfter->duration);
  }

  // The problem with this approach is that duration will appear in two places,
  // but at the moment that doesn't seem important.
  copyParameterList(field.commaSeparated.first().parameters,
                    message->retryAfter->parameters);

  return true;
}


bool parseRouteField(const SIPField &field,
                     std::shared_ptr<SIPMessageHeader> message)
{
  return parseSIPRouteList(field, message->routes);
}


bool parseServerField(const SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->server, false);
}


bool parseSubjectField(const SIPField &field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->subject, true);
}


bool parseSupportedField(const SIPField &field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  if (message->supported == nullptr)
  {
    message->supported = std::shared_ptr<QStringList> (new QStringList);
  }

  parseStringList(field, *message->supported);

  return true;
}


bool parseTimestampField(const SIPField &field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  message->timestamp = std::shared_ptr<SIPTimestamp> (new SIPTimestamp);

  if (!parseFloat(field.commaSeparated.at(0).words.at(0), message->timestamp->timestamp))
  {
    message->timestamp = nullptr;
    return false;
  }

  // optional delay
  if (field.commaSeparated.at(0).words.size() == 2)
  {
    parseFloat(field.commaSeparated.at(0).words.at(1), message->timestamp->delay);
  }

  return true;
}


bool parseToField(const SIPField &field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  if (!parseNameAddr(field.commaSeparated[0].words, message->to.address))
  {
    return false;
  }

  // to-tag does not exist in first message
  parseParameterByName(field.commaSeparated[0].parameters, "tag", message->to.tag);

  return true;
}


bool parseUnsupportedField(const SIPField &field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  return parseStringList(field, message->unsupported);
}


bool parseUserAgentField(const SIPField &field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return parseString(field, message->userAgent, false);
}


bool parseViaField(const SIPField &field,
                   std::shared_ptr<SIPMessageHeader> message)
{
  if (field.commaSeparated[0].words.size() != 2)
  {
    return false;
  }

  ViaField via = {"", NONE, "", 0, "", false, false, 0, "", {}};

  QRegularExpression re_first("SIP/(\\d.\\d)/(\\w+)");
  QRegularExpressionMatch first_match = re_first.match(field.commaSeparated[0].words[0]);

  if(!first_match.hasMatch() || first_match.lastCapturedIndex() != 2)
  {
    return false;
  }

  via.protocol = stringToTransportProtocol(first_match.captured(2));
  via.sipVersion = first_match.captured(1);

  QRegularExpression re_second("([\\w.]+):?(\\d*)");
  QRegularExpressionMatch second_match = re_second.match(field.commaSeparated[0].words[1]);

  if(!second_match.hasMatch() || second_match.lastCapturedIndex() > 2)
  {
    return false;
  }

  via.sentBy = second_match.captured(1);

  if (second_match.lastCapturedIndex() == 2)
  {
    via.port = second_match.captured(2).toUInt();
  }

  parseParameterByName(field.commaSeparated[0].parameters,
      "branch", via.branch);
  parseParameterByName(field.commaSeparated[0].parameters,
      "received", via.receivedAddress);

  QString rportValue = "";
  if (parseParameterByName(field.commaSeparated[0].parameters, "rport", rportValue))
  {
    bool ok = false;
    via.rportValue = rportValue.toUInt(&ok);

    if (!ok)
    {
      via.rportValue = 0;
    }
  }

  message->vias.push_back(via);

  return true;
}


bool parseWarningField(const SIPField &field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  for (auto& value : field.commaSeparated)
  {
    if (value.words.size() == 3)
    {
      uint16_t code = 0;

      if (!parseUint16(value.words.at(0), code) ||
          code < 300 ||
          (code > 307 &&
           code != 330 &&
           code != 331 &&
           code != 370 &&
           code != 399))
      {
        break;
      }

      // the earlier parsing should get rid of quotations
      message->warning.push_back({static_cast<SIPWarningCode> (code),
                                 value.words.at(1),
                                 value.words.at(2)});
    }
  }

  return !message->warning.empty();
}


bool parseWWWAuthenticateField(const SIPField &field,
                               std::shared_ptr<SIPMessageHeader> message)
{
  return parseDigestChallengeField(field, message->wwwAuthenticate);
}
