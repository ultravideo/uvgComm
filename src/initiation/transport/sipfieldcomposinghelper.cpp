#include "sipfieldcomposinghelper.h"

#include "sipconversions.h"

#include "common.h"
#include "logger.h"



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
    Logger::getLogger()->printError("SIP Composing Helper", "Unknown SIP URI type found");
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
    if (nameAddr.realname != "")
    {
      words.pop_back();
    }
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


bool composeSIPRouteLocation(const SIPRouteLocation& location, SIPCommaValue &value)
{
  if (!composeNameAddr(location.address, value.words))
  {
    return false;
  }

  // add location parameters
  if (!location.parameters.empty())
  { 
    for (auto& parameter : location.parameters)
    {
      if (!addParameter(value.parameters, parameter))
      {
        Logger::getLogger()->printProgramWarning("SIP Field Helper",
                            "Failed to add location parameter");
      }
    }
  }

  return true;
}


void composeDigestValue(QString fieldName, const QString& fieldValue, SIPField& field)
{
  if (fieldValue != "")
  {
    field.commaSeparated.push_back({});

    // fieldname=value
    field.commaSeparated.back().words.push_back({fieldName + "=" + fieldValue});
  }
}


void composeDigestValueQuoted(QString fieldName, const QString& fieldValue, SIPField& field)
{
  if (fieldValue != "")
  {
    field.commaSeparated.push_back({});

    // fieldname="value"
    field.commaSeparated.back().words.push_back({fieldName + "=" + "\"" + fieldValue + "\""});
  }
}


bool tryAddParameter(QList<SIPParameter>& parameters,
                     QString parameterName, QString parameterValue)
{
  if (parameterValue == "" || parameterName == "")
  {
    return false;
  }

  parameters.append({parameterName, parameterValue});

  return true;
}


bool tryAddParameter(QList<SIPParameter>& parameters,
                     QString parameterName)
{
  if (parameterName == "")
  {
    return false;
  }

  parameters.append({parameterName, ""});

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
    fields.back().commaSeparated.push_back({{generic.accepted},{}});
    copyParameterList(generic.parameters, fields.back().commaSeparated.back().parameters);
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
      fields.back().commaSeparated.push_back({{value},{}});

      copyParameterList(info.parameters, fields.back().commaSeparated.back().parameters);
    }
    else
    {
      Logger::getLogger()->printProgramWarning("SIP Field Helper", "Failed to compose info abosluteURI");
    }
  }

  // if we failed to add any values. Infos require at least one value
  if (fields.back().commaSeparated.empty())
  {
    fields.pop_back();
    return false;
  }

  return true;
}


bool composeDigestChallengeField(QList<SIPField>& fields,
                                 const QList<DigestChallenge>& dChallenge,
                                 QString fieldname)
{
  if (dChallenge.empty())
  {
    return false;
  }

  for (auto& challenge : dChallenge)
  {
    if(challenge.realm == "")
    {
      return false;
    }

    fields.push_back({fieldname,{}});
    composeDigestValueQuoted("realm",  challenge.realm,                      fields.back());
    if (challenge.domain != nullptr)
    {
      composeDigestValueQuoted("domain", composeAbsoluteURI(*challenge.domain), fields.back());
    }
    composeDigestValueQuoted("nonce",  challenge.nonce,                      fields.back());
    composeDigestValueQuoted("opaque", challenge.opaque,                     fields.back());
    if (challenge.stale != nullptr)
    {
      composeDigestValue      ("stale",  boolToString(*challenge.stale),        fields.back());
    }
    composeDigestValue      ("algorithm", algorithmToString(challenge.algorithm), fields.back());

    QString qopOptions = "";

    for (auto& option : challenge.qopOptions)
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
  }

  return true;
}


bool composeDigestResponseField(QList<SIPField>& fields,
                                const QList<DigestResponse>& dResponse,
                                QString fieldname)
{
  if (dResponse.empty())
  {
    return false;
  }

  for (auto& response : dResponse)
  {
    if (response.username == "" ||
        response.realm == "")
    {
      break;
    }

    fields.push_back({fieldname,{}});

    composeDigestValueQuoted("username", response.username, fields.back());
    composeDigestValueQuoted("realm",    response.realm,    fields.back());
    composeDigestValueQuoted("nonce",    response.nonce, fields.back());

    if (response.digestUri != nullptr)
    {
      composeDigestValueQuoted("uri",     composeSIPURI(*response.digestUri), fields.back());
    }

    composeDigestValueQuoted("response",  response.dresponse, fields.back());
    composeDigestValue      ("algorithm", algorithmToString(response.algorithm), fields.back());

    composeDigestValueQuoted("cnonce",    response.cnonce, fields.back());
    composeDigestValueQuoted("opaque",    response.opaque, fields.back());

    composeDigestValue      ("qop",       qopValueToString(response.messageQop), fields.back());
    composeDigestValue      ("nc",        response.nonceCount, fields.back());

    // make sure we added something successfully and add Digest to beginning
    if (!fields.back().commaSeparated.empty())
    {
      // add word Digest to the beginning
      fields.back().commaSeparated[0].words.push_front("Digest");
    }
    else
    {
      fields.pop_back();
      return false;
    }
  }

  return true;
}


bool composeStringList(QList<SIPField>& fields,
                       const QStringList& list,
                       QString fieldName)
{
  if (list.empty())
  {
    return false;
  }

  fields.push_back({fieldName, QList<SIPCommaValue>{}});

  for (auto& string : list)
  {
    if (string != "")
    {
      fields.back().commaSeparated.push_back({{string},{}});
    }
  }

  if (fields.back().commaSeparated.empty())
  {
    fields.pop_back();
    return false;
  }

  return true;
}


bool composeString(QList<SIPField>& fields,
                   const QString& string,
                   QString fieldName)
{
  if (string == "")
  {
    return false;
  }

  fields.push_back({fieldName, {{{string}, {}}}});
  return true;
}
