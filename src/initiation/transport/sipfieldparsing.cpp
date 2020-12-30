#include "sipfieldparsing.h"

#include "sipconversions.h"
#include "common.h"

#include <QRegularExpression>
#include <QDebug>


// TODO: Support SIPS uri scheme. Needed for TLS
bool parseURI(const QString& word, SIP_URI& uri);
bool parseNameAddr(const QStringList &words, NameAddr& nameAddr);
bool parseSIPRouteLocation(const SIPValueSet &valueSet, SIPRouteLocation& location);

bool parseUritype(QString type, SIPType &out_Type);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint64(QString values, uint64_t& number);
bool parseUint8(QString values, uint8_t& number);

bool preChecks(SIPField& field,
               std::shared_ptr<SIPMessageHeader> message);


bool parseNameAddr(const QStringList &words, NameAddr& nameAddr)
{
  if (words.size() == 0 || words.size() > 2)
  {
    qDebug() << "PEER ERROR: Wrong amount of words in words list for URI. Expected 1-2. Got:"
             << words;
    return false;
  }

  int uriIndex = 0;
  if (words.size() == 2)
  {
    nameAddr.realname = words.at(0);
    uriIndex = 1;
  }
  return parseURI(words.at(uriIndex), nameAddr.uri);
}


bool parseSIPRouteLocation(const SIPValueSet &valueSet, SIPRouteLocation& location)
{
  if (!parseNameAddr(valueSet.words, location.address))
  {
    return false;
  }

  if (valueSet.parameters != nullptr && !valueSet.parameters->empty())
  {
    location.parameters = *valueSet.parameters;
  }
  return true;
}


bool parseURI(const QString &word, SIP_URI& uri)
{
  // for example <sip:bob@biloxi.com>
  // ?: means it wont create a capture group
  // TODO: accept passwords
  QRegularExpression re_field("<(\\w+):(?:(\\w+)@)?(.+)>");
  QRegularExpressionMatch field_match = re_field.match(word);

  // number of matches depends whether real name or the port were given
  if (field_match.hasMatch() &&
      field_match.lastCapturedIndex() == 3)
  {
    QString addressString = "";

    if(field_match.lastCapturedIndex() == 3)
    {
      if (!parseUritype(field_match.captured(1), uri.type))
      {
        return false;
      }

      uri.userinfo.user = field_match.captured(2);
      addressString = field_match.captured(3);
    }

    QStringList parameters = addressString.split(";", QString::SkipEmptyParts);

    if (!parameters.empty())
    {
      const int firstParameterIndex = 1;
      if (parameters.size() > firstParameterIndex)
      {
        for (int i = firstParameterIndex; i < parameters.size(); ++i)
        {
          // currently parsing doesn't do any further processing on URI parameters
          uri.uri_parameters.push_back(SIPParameter());
          parseParameter(parameters.at(i), uri.uri_parameters.back());
        }
      }

      // TODO: improve this to accept IPv4, Ipv6 and addresses
      QRegularExpression re_address("(\\[.+\\]|[\\w.]+):?(\\d*)");
      QRegularExpressionMatch address_match = re_address.match(parameters.first());

      if(address_match.hasMatch() &&
         address_match.lastCapturedIndex() >= 1 &&
         address_match.lastCapturedIndex() <= 2)
      {
        uri.hostport.host = address_match.captured(1);

        if (address_match.lastCapturedIndex() == 2 && address_match.captured(2) != "")
        {
          uri.hostport.port = address_match.captured(2).toUInt();
        }
        else
        {
          uri.hostport.port = 0;
        }
      }
      else
      {
        return false;
      }
    }

    return true;
  }

  return false;
}


bool parseUritype(QString type, SIPType& out_Type)
{
  if (type == "sip")
  {
    out_Type = SIP;
  }
  else if (type == "sips")
  {
    out_Type = SIPS;
  }
  else if (type == "tel")
  {
    out_Type = TEL;
  }
  else
  {
    qDebug() << "ERROR:  Could not identify connection type:" << type;
    return false;
  }

  return true;
}


bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value)
{
  if(parameters != nullptr)
  {
    for(SIPParameter parameter : *parameters)
    {
      if(parameter.name == name)
      {
        value = parameter.value;
        return true;
      }
    }
  }
  return false;
}


bool parseUint64(QString values, uint64_t& number)
{
  QRegularExpression re_field("(\\d+)");
  QRegularExpressionMatch field_match = re_field.match(values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    bool ok = false;
    uint64_t parsedNumber = values.toULongLong(&ok);

    if (!ok)
    {
      return false;
    }

    number = parsedNumber;
    return true;
  }
  return false;
}

bool parseUint8(QString values, uint8_t& number)
{
  uint64_t parsed = 0;

  if (!parseUint64(values, parsed) ||
      parsed > UINT8_MAX)
  {
    return false;
  }

  number = (uint8_t)parsed;
  return true;
}


bool preChecks(SIPField& field,
               std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message != nullptr);
  Q_ASSERT(!field.valueSets.empty());
  Q_ASSERT(!field.valueSets[0].words.empty());

  if (message == nullptr ||
      field.valueSets.empty() ||
      field.valueSets[0].words.empty())
  {
    printProgramError("SIPFieldParsing", "Parsing prechecks failed");
    return false;
  }
  return true;
}



bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message) ||
      !parseNameAddr(field.valueSets[0].words, message->to.address))
  {
    return false;
  }

  // to-tag does not exist in first message
  parseParameterNameToValue(field.valueSets[0].parameters, "tag", message->to.tag);

  return true;
}


bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message) ||
      !parseNameAddr(field.valueSets[0].words, message->from.address))
  {
    return false;
  }

  // from tag should always be included
  return parseParameterNameToValue(field.valueSets[0].parameters, "tag", message->from.tag);
}


bool parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message) ||
      field.valueSets[0].words.size() != 2)
  {
    return false;
  }

  bool ok = false;

  message->cSeq.cSeq = field.valueSets[0].words[0].toUInt(&ok);
  message->cSeq.method = stringToRequest(field.valueSets[0].words[1]);
  return message->cSeq.method != SIP_NO_REQUEST && ok;
}


bool parseCallIDField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (!preChecks(field, message) ||
      field.valueSets[0].words.size() != 1)
  {
    return false;
  }

  message->callID = field.valueSets[0].words[0];
  return true;
}


bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message) ||
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

  via.protocol = stringToConnection(first_match.captured(2));
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


bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message) ||
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


bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message) ||
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


bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  if (!preChecks(field, message))
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


bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageHeader> message)
{
  return preChecks(field, message) &&
      field.valueSets[0].words.size() == 1 &&
      parseUint64(field.valueSets[0].words[0], message->contentLength);
}


bool parseRecordRouteField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

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


bool parseServerField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() < 1
      || field.valueSets[0].words.size() > 100)
  {
    return false;
  }

  message->server.push_back(field.valueSets[0].words[0]);
  return true;
}


bool parseUserAgentField(SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() < 1
      || field.valueSets[0].words.size() > 100)
  {
    return false;
  }

  message->userAgent.push_back(field.valueSets[0].words[0]);
  return true;
}


bool parseUnimplemented(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  printUnimplemented("SIPFieldParsing", "Found unsupported SIP field type: " + field.name);

  return false;
}

int countVias(QList<SIPField> &fields)
{
  int vias = 0;
  for(SIPField& field : fields)
  {
    if(field.name == "Via")
    {
      ++vias;
    }
  }

  return vias;
}


bool isLinePresent(QString name, QList<SIPField>& fields)
{
  for(SIPField& field : fields)
  {
    if(field.name == name)
    {
      return true;
    }
  }
  qDebug() << "Did not find header:" << name;
  return false;
}


bool parseParameter(QString text, SIPParameter& parameter)
{
  QRegularExpression re_parameter("([^=]+)=?([^;]*)");
  QRegularExpressionMatch parameter_match = re_parameter.match(text);

  if(parameter_match.hasMatch() && parameter_match.lastCapturedIndex() == 2)
  {
    parameter.name = parameter_match.captured(1);

    if (parameter_match.captured(2) != "")
    {
      parameter.value = parameter_match.captured(2);
    }

    return true;
  }

  return false;
}

