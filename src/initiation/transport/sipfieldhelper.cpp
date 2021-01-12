#include "sipfieldhelper.h"

#include "sipconversions.h"

#include "common.h"

#include <QRegularExpression>



// ================== Composing functions ==================

QString composeURItype(SIPType type)
{
  if (type == SIP)
  {
    return "sip:";
  }
  else if (type == SIPS)
  {
    return "sips:";
  }
  else if (type == TEL)
  {
    return "tel:";
  }
  else
  {
    printError("SIP Composing Helper", "Unknown SIP URI type found");
  }

  return "";
}


QString composePortString(uint16_t port)
{
  QString portString = "";

  if (port != 0)
  {
    portString = ":" + QString::number(port);
  }

  return portString;
}


bool composeNameAddr(const NameAddr& nameAddr, QStringList& words)
{
  if (nameAddr.realname != "")
  {
    words.push_back("\"" + nameAddr.realname + "\"");
  }

  QString uri = "<" + composeSIPURI(nameAddr.uri) + ">";

  if (uri == "<>")
  {
    words.pop_back();
    return false;
  }

  words.push_back(uri);

  return true;
}


QString composeSIPURI(const SIP_URI &uri)
{
  QString uriString = composeURItype(uri.type);
  if (uriString != "")
  {
    QString parameters = "";

    for (auto& parameter : uri.uri_parameters)
    {
      parameters += ";" + parameter.name;
      if (parameter.value != "")
      {
        parameters += "=" + parameter.value;
      }
    }

    QString usernameString = "";

    if (uri.userinfo.user != "")
    {
      usernameString = uri.userinfo.user;

      if (uri.userinfo.password != "")
      {
        usernameString += ":" + uri.userinfo.password;
      }
      usernameString += "@";
    }

    uriString += usernameString + uri.hostport.host
        + composePortString(uri.hostport.port) + parameters;

    return uriString;
  }
  return "";
}


QString composeAbsoluteURI(const AbsoluteURI& uri)
{
  if (uri.scheme == "" || uri.path == "")
  {
    return "";
  }

  return uri.scheme + ":" + uri.path;
}


bool composeSIPRouteLocation(const SIPRouteLocation& location, SIPValueSet &valueSet)
{
  if (!composeNameAddr(location.address, valueSet.words))
  {
    return false;
  }

  if (!location.parameters.empty())
  {
    valueSet.parameters
        = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
    *valueSet.parameters = location.parameters;
  }
  return true;
}


void composeDigestValue(QString fieldName, const QString& fieldValue, SIPField& field)
{
  if (fieldValue != "")
  {
    field.valueSets.push_back({});

    // fieldname=value
    field.valueSets.back().words.push_back({fieldName + "=" + fieldValue});
  }
}


void composeDigestValueQuoted(QString fieldName, const QString& fieldValue, SIPField& field)
{
  if (fieldValue != "")
  {
    field.valueSets.push_back({});

    // fieldname="value"
    field.valueSets.back().words.push_back({fieldName + "=" + "\"" + fieldValue + "\""});
  }
}


bool tryAddParameter(std::shared_ptr<QList<SIPParameter>>& parameters,
                     QString parameterName, QString parameterValue)
{
  if (parameterValue == "" || parameterName == "")
  {
    return false;
  }

  if (parameters == nullptr)
  {
    parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  parameters->append({parameterName, parameterValue});

  return true;
}


bool tryAddParameter(std::shared_ptr<QList<SIPParameter>>& parameters,
                     QString parameterName)
{
  if (parameterName == "")
  {
    return false;
  }

  if (parameters == nullptr)
  {
    parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  parameters->append({parameterName, ""});

  return true;
}


bool addParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                  const SIPParameter& parameter)
{
  if (parameter.name == "")
  {
    return false;
  }

  if (parameters == nullptr)
  {
    parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }

  parameters->append(parameter);
  return true;
}


bool composeAcceptGenericField(QList<SIPField>& fields,
                          const std::shared_ptr<QList<SIPAcceptGeneric>> generics,
                          QString fieldname)
{
  if (generics == nullptr)
  {
    return false;
  }

  fields.push_back({fieldname,{}});

  // compose each comma(,) separated accept value
  for (auto& generic : *generics)
  {
    fields.back().valueSets.push_back({{generic.accepted},{}});
    if (generic.parameter != nullptr)
    {
      if (!addParameter(fields.back().valueSets.back().parameters, *generic.parameter))
      {
        printProgramWarning("SIP Field Composing", "Failed to add parameter");
      }
    }
  }

  return true;
}


bool composeInfoField(QList<SIPField>& fields,
                      const QList<SIPInfo>& infos,
                      QString fieldname)
{
  if (infos.empty())
  {
    return false;
  }

  fields.push_back({fieldname,{}});

  for (auto& info : infos)
  {
    QString value = "<" + composeAbsoluteURI(info.absoluteURI) + ">";

    if (value != "<>")
    {
      fields.back().valueSets.push_back({{value},{}});
      if (info.parameter != nullptr)
      {
        if (!addParameter(fields.back().valueSets.back().parameters, *info.parameter))
        {
          printProgramWarning("SIP Field Helper", "Failed to add Info parameter");
        }
      }
    }
    else
    {
      printProgramWarning("SIP Field Helper", "Failed to compose info abosluteURI");
    }
  }

  // if we failed to add any valuesets. Infos require at least one value
  if (fields.back().valueSets.empty())
  {
    fields.pop_back();
    return false;
  }

  return true;
}


bool composeDigestChallengeField(QList<SIPField>& fields,
                                 const std::shared_ptr<DigestChallenge> dChallenge,
                                 QString fieldname)
{
  if (dChallenge == nullptr ||
      dChallenge->realm == "")
  {
    return false;
  }

  fields.push_back({fieldname,{}});
  composeDigestValueQuoted("realm",  dChallenge->realm,                      fields.back());
  composeDigestValueQuoted("domain", composeAbsoluteURI(dChallenge->domain), fields.back());
  composeDigestValueQuoted("nonce",  dChallenge->nonce,                      fields.back());
  composeDigestValueQuoted("opaque", dChallenge->opaque,                     fields.back());
  composeDigestValue      ("stale",  boolToString(dChallenge->stale),        fields.back());
  composeDigestValue      ("algorithm", algorithmToString(dChallenge->algorithm), fields.back());

  QString qopOptions = "";

  for (auto& option : dChallenge->qopOptions)
  {
    if (qopOptions != "" &&
        qopOptions.right(1) != ",")
    {
      qopOptions += ",";
    }

    qopOptions += qopValueToString(option);
  }

  if (qopOptions.right(1) == ",")
  {
    qopOptions = qopOptions.left(qopOptions.length() -1);
  }

  composeDigestValueQuoted("qop", qopOptions, fields.back());

  return true;
}


bool composeDigestResponseField(QList<SIPField>& fields,
                                const std::shared_ptr<DigestResponse> dResponse,
                                QString fieldname)
{
  if (dResponse == nullptr ||
      dResponse->username == "" ||
      dResponse->realm == "")
  {
    return false;
  }

  fields.push_back({fieldname,{}});

  composeDigestValueQuoted("username", dResponse->username, fields.back());
  composeDigestValueQuoted("realm",    dResponse->realm,    fields.back());
  composeDigestValueQuoted("nonce",    dResponse->nonce, fields.back());

  if (dResponse->digestUri != nullptr)
  {
    composeDigestValueQuoted("uri",     composeSIPURI(*dResponse->digestUri), fields.back());
  }

  composeDigestValueQuoted("response",    dResponse->dresponse, fields.back());
  composeDigestValue      ("algorithm", algorithmToString(dResponse->algorithm), fields.back());

  composeDigestValueQuoted("cnonce",    dResponse->cnonce, fields.back());
  composeDigestValueQuoted("opaque",    dResponse->opaque, fields.back());

  composeDigestValue      ("qop",       qopValueToString(dResponse->messageQop), fields.back());
  composeDigestValue      ("nc",        dResponse->nonceCount, fields.back());

  // make sure we added something successfully and add Digest to beginning
  if (!fields.back().valueSets.empty())
  {
    // add word Digest to the beginning
    fields.back().valueSets[0].words.push_front("Digest");
  }
  else
  {
    fields.pop_back();
    return false;
  }

  return true;
}





// ================== Parsing functions ==================


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


bool parsingPreChecks(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message,
                      bool emptyPossible)
{
  Q_ASSERT(message != nullptr);
  if (!emptyPossible)
  {
    Q_ASSERT(!field.valueSets.empty());
  }

  if (message == nullptr ||
      (!emptyPossible && field.valueSets.empty()))
  {
    printProgramError("SIPFieldParsing", "Parsing prechecks failed");
    return false;
  }

  // Check that none of the valuesets have no words.
  // Empty fields should be handled with having no valuesets.
  for (auto& valueset: field.valueSets)
  {
    Q_ASSERT(!valueset.words.empty());

    if (valueset.words.empty())
    {
      printProgramError("SIPFieldParsing", "Found empty valueset");
      return false;
    }
  }

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

