#include "sipfieldparsinghelper.h"

#include "sipconversions.h"
#include "common.h"

#include <QRegularExpression>



bool parseNameAddr(const QStringList &words, NameAddr& nameAddr)
{
  if (words.size() == 0 || words.size() > 2)
  {
    printPeerError("SIP Field Helper", "Wrong amount of words in words list for URI. Expected 1-2",
                   "Words", QString::number(words.size()));

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


bool parseSIPRouteLocation(const SIPCommaValue &value, SIPRouteLocation& location)
{
  if (!parseNameAddr(value.words, location.address))
  {
    return false;
  }

  if (value.parameters != nullptr && !value.parameters->empty())
  {
    location.parameters = *value.parameters;
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
    printError("Could not identify uri scheme", "Scheme", type);

    return false;
  }

  return true;
}


bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value)
{
  if(parameters != nullptr)
  {
    for(SIPParameter& parameter : *parameters)
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


bool parseAcceptGeneric(SIPField& field,
                        std::shared_ptr<QList<SIPAcceptGeneric>> generics)
{
  if (generics == nullptr)
  {
    generics = std::shared_ptr<QList<SIPAcceptGeneric>> (new QList<SIPAcceptGeneric>());
  }

  for (auto& value : field.commaSeparated)
  {
    if (value.words.size() == 1)
    {
      // parse accepted media type
      QString accept = value.words.first();

      // if we recognize this type
      if (accept.length() > 0)
      {
        // add it to struct
        generics->push_back({accept, {}});
        copyParameterList(value.parameters, generics->back().parameters);
      }
    }
  }

  return false;
}
