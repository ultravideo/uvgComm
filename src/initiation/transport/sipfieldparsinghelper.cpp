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

  QRegularExpression re_uri("<([\\w\\d]+:.+)>");
  QRegularExpressionMatch uri_match = re_uri.match(words.at(uriIndex));

  if (uri_match.hasMatch() && uri_match.lastCapturedIndex() == 1)
  {
    return parseURI(uri_match.captured(1), nameAddr.uri);
  }

  return false;
}


bool parseSIPRouteLocation(const SIPCommaValue &value, SIPRouteLocation& location)
{
  if (!parseNameAddr(value.words, location.address))
  {
    return false;
  }

  copyParameterList(value.parameters, location.parameters);

  return true;
}


bool parseSIPRouteList(const SIPField& field, QList<SIPRouteLocation>& list)
{
  if (field.commaSeparated[0].words.size() != 1)
  {
    return false;
  }

  for (auto& value : field.commaSeparated)
  {
    list.push_back(SIPRouteLocation{{"", SIP_URI{}}, {}});
    if (!parseSIPRouteLocation(value, list.back()))
    {
      list.pop_back();
      return false;
    }
  }

  return true;
}


bool parseURI(const QString &word, SIP_URI& uri)
{
  // for example <sip:bob@biloxi.com>
  // ?: means it wont create a capture group
  // TODO: accept passwords
  QRegularExpression re_field("(\\w+):(?:(\\w+)@)?(.+)");
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


bool parseAbsoluteURI(const QString& word, AbsoluteURI& uri)
{
  QRegularExpression re_parameter("(.*):(.*)");
  QRegularExpressionMatch parameter_match = re_parameter.match(word);

  if(parameter_match.hasMatch() && parameter_match.lastCapturedIndex() == 2)
  {
    uri.scheme = parameter_match.captured(1);
    uri.path = parameter_match.captured(2);

    return true;
  }

  return false;
}


bool parseUritype(const QString &type, SIPType& out_Type)
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


bool parseParameterByName(QList<SIPParameter>& parameters,
                          QString name, QString& value)
{
  for (int i = 0; i < parameters.size(); ++i)
  {
    if (parameters.at(i).name == name)
    {
      value = parameters.at(i).value;
      parameters.erase(parameters.begin() + i);
      return true;
    }
  }

  return false;
}


bool parseFloat(const QString& string, float& value)
{
  bool ok = false;
  value = string.toFloat(&ok);

  return ok;
}


bool parseUint64(const QString &values, uint64_t& number)
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


bool parseUint8(const QString &values, uint8_t& number)
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


bool parseUint16(const QString &values, uint16_t& number)
{
  uint64_t parsed = 0;

  if (!parseUint64(values, parsed) ||
      parsed > UINT16_MAX)
  {
    return false;
  }

  number = (uint16_t)parsed;
  return true;
}


bool parseUint32(const QString &values, uint32_t& number)
{
  uint64_t parsed = 0;

  if (!parseUint64(values, parsed) ||
      parsed > UINT32_MAX)
  {
    return false;
  }

  number = (uint32_t)parsed;
  return true;
}


bool parseSharedUint32(const SIPField &field, std::shared_ptr<uint32_t>& value)
{
  value = std::shared_ptr<uint32_t> (new uint32_t);

  if (!parseUint32(field.commaSeparated.at(0).words.at(0), *value))
  {
    value = nullptr;
    return false;
  }

  return true;
}


bool parseUintMax(const QString values, uint64_t& number, const uint64_t& maximum)
{
  uint64_t parsed = 0;

  if (!parseUint64(values, parsed) ||
      parsed > maximum)
  {
    return false;
  }

  number = (uint8_t)parsed;
  return true;
}


bool parseParameter(const QString &text, SIPParameter& parameter)
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


bool parseAcceptGeneric(const SIPField& field,
                        std::shared_ptr<QList<SIPAcceptGeneric>> generics)
{
  if (generics == nullptr)
  {
    generics =
        std::shared_ptr<QList<SIPAcceptGeneric>> (new QList<SIPAcceptGeneric>());
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


bool parseInfo(const SIPField &field,
               QList<SIPInfo>& infos)
{
  for (auto& value : field.commaSeparated)
  {
    if (value.words.size() == 1)
    {
      QRegularExpression re_uri("<(.*)>");
      QRegularExpressionMatch uri_match = re_uri.match(value.words[0]);

      if(uri_match.hasMatch() &&
         uri_match.lastCapturedIndex() == 1)
      {
        AbsoluteURI uri;
        if (parseAbsoluteURI(uri_match.captured(1), uri))
        {
          infos.push_back({uri, {}});

          copyParameterList(value.parameters, infos.back().parameters);
        }
      }
    }
  }

  return true;
}


bool parseDigestValue(const QString& word, QString& name, QString& value)
{
  QRegularExpression re_quoted("(.*)=\"(.*)\"");
  QRegularExpressionMatch quoted_match = re_quoted.match(word);

  if(quoted_match.hasMatch() && quoted_match.lastCapturedIndex() == 2)
  {
    name = quoted_match.captured(1);
    value = quoted_match.captured(2);

    return true;
  }

  QRegularExpression re_normal("(.*)=(.*)");
  QRegularExpressionMatch normal_match = re_normal.match(word);

  if(normal_match.hasMatch() && normal_match.lastCapturedIndex() == 2)
  {
    name = normal_match.captured(1);
    value = normal_match.captured(2);

    return true;
  }

  return false;
}


bool parseDigestChallengeField(const SIPField& field,
                               QList<DigestChallenge>& dChallenge)
{
  dChallenge.push_back(DigestChallenge{});

  std::map<QString, QString> digests;
  populateDigestTable(field.commaSeparated, digests, true);

  dChallenge.back().realm = getDigestTableValue(digests, "realm");

  QString domain = getDigestTableValue(digests, "domain");
  if (domain != "")
  {
    dChallenge.back().domain = std::shared_ptr<AbsoluteURI>(new AbsoluteURI);
    if (parseAbsoluteURI(domain, *dChallenge.back().domain))
    {
      dChallenge.back().domain = nullptr;
    }
  }

  dChallenge.back().nonce = getDigestTableValue(digests, "nonce");
  dChallenge.back().opaque = getDigestTableValue(digests, "opaque");

  QString stale = getDigestTableValue(digests, "stale");
  if (stale != "")
  {
    bool success = false;
    dChallenge.back().stale = std::shared_ptr<bool> (new bool (stringToBool(stale, success)));

    if (!success)
    {
      dChallenge.back().stale = nullptr;
    }
  }

  dChallenge.back().algorithm = stringToAlgorithm(getDigestTableValue(digests, "algorithm"));

  // parsing of QOP options
  QString gopOptions = getDigestTableValue(digests, "qop");

  // non-capture group to separate commas(,)
  QRegularExpression re_options ("(?:([\\w-]*),?)");
  QRegularExpressionMatch options_match = re_options.match(gopOptions);

  if (options_match.hasMatch() &&
      options_match.lastCapturedIndex() >= 1)
  {
    // the whole string is at 0 so we ignore it
    for (int i = 1; i < options_match.lastCapturedIndex(); ++i)
    {
      QopValue qop = stringToQopValue(options_match.captured(i));

      if (qop != SIP_NO_AUTH &&
          qop != SIP_AUTH_UNKNOWN)
      {
        // success
        dChallenge.back().qopOptions.push_back(qop);
      }
    }
  }

  return true;
}


bool parseDigestResponseField(const SIPField& field,
                             QList<DigestResponse>& dResponse)
{
  if (field.commaSeparated.empty() ||
      field.commaSeparated.first().words.size() != 2 ||
      field.commaSeparated.first().words.at(0) != "Digest")
  {
    return false;
  }

  dResponse.push_back(DigestResponse{});

  std::map<QString, QString> digests;
  populateDigestTable(field.commaSeparated, digests, true);

  dResponse.back().username = getDigestTableValue(digests, "username");
  dResponse.back().realm    = getDigestTableValue(digests, "realm");
  dResponse.back().nonce    = getDigestTableValue(digests, "nonce");

  QString uri = getDigestTableValue(digests, "uri");

  if (uri != "")
  {
    dResponse.back().digestUri = std::shared_ptr<SIP_URI> (new SIP_URI);

    if (!parseURI(uri, *dResponse.back().digestUri))
    {
      printWarning("SIP Field Parsing", "Failed to parse Digest response URI");
      dResponse.back().digestUri = nullptr;
    }
  }

  dResponse.back().dresponse = getDigestTableValue(digests, "response");
  dResponse.back().algorithm = stringToAlgorithm(getDigestTableValue(digests, "algorithm"));
  dResponse.back().cnonce = getDigestTableValue(digests, "cnonce");
  dResponse.back().opaque = getDigestTableValue(digests, "opaque");

  dResponse.back().messageQop = stringToQopValue(getDigestTableValue(digests, "qop"));

  dResponse.back().nonceCount = getDigestTableValue(digests, "nc");

  return true;
}


void populateDigestTable(const QList<SIPCommaValue>& values,
                         std::map<QString, QString>& table, bool expectDigest)
{
  for (int i = 0; i < values.size(); ++i)
  {
    // The first value will may two words, first of which is "Digest"
    if ((i == 0 && expectDigest &&
         (values.at(i).words.size() != 2 || values.at(i).words.at(0) != "Digest")) ||
        (i > 0  && values.at(i).words.size() != 1 ))
    {
      break;
    }

    int wordIndex = 0;
    if (i == 0 && expectDigest)
    {
      wordIndex = 1; // take Digest into account
    }

    QString digestName = "";
    QString digestValue = "";

    parseDigestValue(values.at(i).words.at(wordIndex), digestName, digestValue);

    if (digestName != "")
    {
      table[digestName] = digestValue;
    }
  }
}


QString getDigestTableValue(const std::map<QString, QString>& table, const QString& name)
{
  if (table.find(name) == table.end())
  {
    return "";
  }
  return table.at(name);
}


bool parseStringList(const SIPField& field, QStringList& list)
{
  for (auto& encoding : field.commaSeparated)
  {
    list.push_back(encoding.words.first());
  }

  return !field.commaSeparated.empty();
}


bool parseString(const SIPField& field, QString& value, bool allowEmpty)
{
  if (!allowEmpty && field.commaSeparated[0].words.size() != 1)
  {
    return false;
  }

  // words size is prechecked, but sometimes empty comma separated is possible
  if (field.commaSeparated.size() == 1)
  {
    value = field.commaSeparated[0].words[0];
  }

  return true;
}


bool parseFromTo(const SIPField &field, ToFrom& fromTo)
{
  if (!parseNameAddr(field.commaSeparated[0].words, fromTo.address))
  {
    return false;
  }

  copyParameterList(field.commaSeparated[0].parameters, fromTo.parameters);

  parseParameterByName(fromTo.parameters,
                                "tag", fromTo.tagParameter);
  return true;
}
