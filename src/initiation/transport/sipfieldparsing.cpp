#include "sipfieldparsing.h"


#include "sipfieldhelper.h"
#include "sipconversions.h"
#include "common.h"

#include <QRegularExpression>
#include <QDebug>




bool parseAcceptField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseAcceptEncodingField(SIPField& field,
                              std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseAcceptLanguageField(SIPField& field,
                              std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseAlertInfoField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseAllowField(SIPField& field,
                     std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseAuthInfoField(SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseAuthorizationField(SIPField& field,
                             std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseCallIDField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (!parsingPreChecks(field, message) ||
      field.valueSets[0].words.size() != 1)
  {
    return false;
  }

  message->callID = field.valueSets[0].words[0];
  return true;
}


bool parseCallInfoField(SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message) ||
      field.valueSets[0].words.size() != 1)
  {
    return false;
  }

  for(auto& valueSet : field.valueSets)
  {
    message->contact.push_back(SIPRouteLocation());

    // TODO: parse parameters
    if(!parseNameAddr(valueSet.words, message->contact.back().address))
    {
      message->contact.pop_back();
      return false;
    }
  }

  return true;
}


bool parseContentDispositionField(SIPField& field,
                                  std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseContentEncodingField(SIPField& field,
                               std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseContentLanguageField(SIPField& field,
                               std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageHeader> message)
{
  return parsingPreChecks(field, message) &&
      field.valueSets[0].words.size() == 1 &&
      parseUint64(field.valueSets[0].words[0], message->contentLength);
}


bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message))
  {
    return false;
  }

  QRegularExpression re_field("(\\w+/\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.valueSets[0].words[0]);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    message->contentType = stringToContentType(field_match.captured(1));
    return true;
  }
  return false;
}


bool parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message) ||
      field.valueSets[0].words.size() != 2)
  {
    return false;
  }

  bool ok = false;

  message->cSeq.cSeq = field.valueSets[0].words[0].toUInt(&ok);
  message->cSeq.method = stringToRequestMethod(field.valueSets[0].words[1]);
  return message->cSeq.method != SIP_NO_REQUEST && ok;
}


bool parseDateField(SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseErrorInfoField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseExpireField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message) ||
      !parseNameAddr(field.valueSets[0].words, message->from.address))
  {
    return false;
  }

  // from tag should always be included
  return parseParameterNameToValue(field.valueSets[0].parameters, "tag", message->from.tag);
}


bool parseInReplyToField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message) ||
      field.valueSets[0].words.size() != 1)
  {
    return false;
  }

  uint8_t value = 0;

  if (!parseUint8(field.valueSets[0].words[0], value))
  {
    return false;
  }

  field.valueSets[0].words[0], message->maxForwards = std::shared_ptr<uint8_t> (new uint8_t{value});
  return true;
}


bool parseMinExpiresField(SIPField& field,
                          std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseMIMEVersionField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseOrganizationField(SIPField& field,
                            std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parsePriorityField(SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseProxyAuthenticateField(SIPField& field,
                                 std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseProxyAuthorizationField(SIPField& field,
                                  std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseProxyRequireField(SIPField& field,
                            std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseRecordRouteField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message))
  {
    return false;
  }

  for (auto& valueSet : field.valueSets)
  {
    message->recordRoutes.push_back(SIPRouteLocation{{"", SIP_URI{}}, {}});
    if (parseSIPRouteLocation(valueSet, message->recordRoutes.back()))
    {
      return false;
    }
  }
  return true;
}


bool parseReplyToField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseRequireField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseRetryAfterField(SIPField& field,
                          std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseRouteField(SIPField& field,
                     std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseServerField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message))
  {
    return false;
  }

  if (field.valueSets[0].words.size() < 1
      || field.valueSets[0].words.size() > 100)
  {
    return false;
  }

  message->server.push_back(field.valueSets[0].words[0]);
  return true;
}


bool parseSubjectField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseSupportedField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseTimestampField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message) ||
      !parseNameAddr(field.valueSets[0].words, message->to.address))
  {
    return false;
  }

  // to-tag does not exist in first message
  parseParameterNameToValue(field.valueSets[0].parameters, "tag", message->to.tag);

  return true;
}


bool parseUnsupportedField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseUserAgentField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message))
  {
    return false;
  }

  if (field.valueSets[0].words.size() < 1
      || field.valueSets[0].words.size() > 100)
  {
    return false;
  }

  message->userAgent.push_back(field.valueSets[0].words[0]);
  return true;
}


bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageHeader> message)
{
  if (!parsingPreChecks(field, message) ||
      field.valueSets[0].words.size() != 2)
  {
    return false;
  }

  ViaField via = {"", NONE, "", 0, "", false, false, 0, "", {}};

  QRegularExpression re_first("SIP/(\\d.\\d)/(\\w+)");
  QRegularExpressionMatch first_match = re_first.match(field.valueSets[0].words[0]);

  if(!first_match.hasMatch() || first_match.lastCapturedIndex() != 2)
  {
    return false;
  }

  via.protocol = stringToTransportProtocol(first_match.captured(2));
  via.sipVersion = first_match.captured(1);

  QRegularExpression re_second("([\\w.]+):?(\\d*)");
  QRegularExpressionMatch second_match = re_second.match(field.valueSets[0].words[1]);

  if(!second_match.hasMatch() || second_match.lastCapturedIndex() > 2)
  {
    return false;
  }

  via.sentBy = second_match.captured(1);

  if (second_match.lastCapturedIndex() == 2)
  {
    via.port = second_match.captured(2).toUInt();
  }

  parseParameterNameToValue(field.valueSets[0].parameters, "branch", via.branch);
  parseParameterNameToValue(field.valueSets[0].parameters, "received", via.receivedAddress);

  QString rportValue = "";
  if (parseParameterNameToValue(field.valueSets[0].parameters, "rport", rportValue))
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


bool parseWarningField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}


bool parseWWWAuthenticateField(SIPField& field,
                               std::shared_ptr<SIPMessageHeader> message)
{
  return false;
}
