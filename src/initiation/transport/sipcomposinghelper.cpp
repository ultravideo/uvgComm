#include "sipcomposinghelper.h"

#include "common.h"


QString composeUritype(SIPType type)
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

  return composeSIPUri(nameAddr.uri, words);
}

bool composeSIPUri(const SIP_URI& uri, QStringList& words)
{

  QString uriString = "<" + composeUritype(uri.type);
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
        + composePortString(uri.hostport.port) + parameters + ">";

    words.push_back(uriString);

    return true;
  }
  return false;
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


