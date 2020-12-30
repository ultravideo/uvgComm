#include "sipfieldcomposing.h"

#include "sipconversions.h"

#include "common.h"

#include <QDebug>


// a function used within this file to add a parameter
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName, QString parameterValue);
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName);

QString composeUritype(SIPType type);
bool composeSIPUri(SIP_URI& uri, QStringList& words);
bool composeNameAddr(NameAddr& nameAddr, QStringList& words);

bool composeSIPRouteLocation(SIPRouteLocation& location, SIPValueSet& valueSet);

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
    qDebug() << "ERROR:  Unset connection type detected while composing SIP message";
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

bool composeNameAddr(NameAddr& nameAddr, QStringList& words)
{
  if (nameAddr.realname != "")
  {
    words.push_back("\"" + nameAddr.realname + "\"");
  }

  return composeSIPUri(nameAddr.uri, words);
}

bool composeSIPUri(SIP_URI& uri, QStringList& words)
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


bool composeSIPRouteLocation(SIPRouteLocation& location, SIPValueSet &valueSet)
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


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding)
{
  if(request.requestURI.hostport.host == "")
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPComposing", 
               "Request URI host is empty when comprising the first line.");
  }

  if(request.method == SIP_NO_REQUEST)
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPComposing",
                "SIP_NO_REQUEST given.");
    return false;
  }

  QString type = "";
  QString target = "";
  QString port = "";

  if (request.requestURI.hostport.port != 0)
  {
    port = ":" + QString::number(request.requestURI.hostport.port) + ";transport=tcp";
  }


  if(request.method != SIP_REGISTER)
  {
    type = composeUritype(request.requestURI.type);
    target = request.requestURI.userinfo.user + "@" + request.requestURI.hostport.host;
  }
  else // REGISTER first line does not contain username.
  {
    type = composeUritype(request.requestURI.type);
    target = request.requestURI.hostport.host;
  }

  line = requestToString(request.method) + " " + type
      + target + port + " SIP/" + request.sipVersion + lineEnding;

  return true;
}

bool getFirstResponseLine(QString& line, SIPResponse& response,
                          QString lineEnding)
{
  if(response.type == SIP_UNKNOWN_RESPONSE)
  {
    qDebug() << "WARNING: First response line failed";
    return false;
  }
  line = "SIP/" + response.sipVersion + " "
      + QString::number(responseToCode(response.type)) + " "
      + responseToPhrase(response.type) + lineEnding;
  return true;
}

bool includeToField(QList<SIPField> &fields,
                    std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message->to.address.uri.userinfo.user != "" &&
      message->to.address.uri.hostport.host != "");
  if(message->to.address.uri.userinfo.user == "" ||
     message->to.address.uri.hostport.host == "")
  {
    qDebug() << "WARNING: Composing To-field failed because host is:"
             << message->to.address.uri.hostport.host << "and" << message->to.address.uri.userinfo.user;
    return false;
  }

  SIPField field = {"To", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  if (!composeNameAddr(message->to.address, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", message->to.tag);

  fields.push_back(field);
  return true;
}

bool includeFromField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message->from.address.uri.userinfo.user != "" &&
      message->from.address.uri.hostport.host != "");
  if(message->from.address.uri.userinfo.user == "" ||
     message->from.address.uri.hostport.host == "")
  {
    qDebug() << "WARNING: From field failed";
    return false;
  }

  SIPField field = {"From", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  if (!composeNameAddr(message->from.address, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", message->from.tag);

  fields.push_back(field);
  return true;
}

bool includeCSeqField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message->cSeq.cSeq != 0 && message->cSeq.method != SIP_NO_REQUEST);
  if(message->cSeq.cSeq == 0 || message->cSeq.method == SIP_NO_REQUEST)
  {
    qDebug() << "WARNING: cSeq field failed";
    return false;
  }

  SIPField field = {"CSeq", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  field.valueSets[0].words.push_back(QString::number(message->cSeq.cSeq));
  field.valueSets[0].words.push_back(requestToString(message->cSeq.method));
  field.valueSets[0].parameters = nullptr;

  fields.push_back(field);
  return true;
}

bool includeCallIDField(QList<SIPField> &fields,
                        std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message->callID != "");
  if(message->callID == "")
  {
    qDebug() << "WARNING: Call-ID field failed";
    return false;
  }

  SIPField field = {"Call-ID", QList<SIPValueSet>{SIPValueSet{{message->callID}, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeViaFields(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(!message->vias.empty());
  if(message->vias.empty())
  {
    qDebug() << "WARNING: Via field failed";
    return false;
  }

  for(ViaField& via : message->vias)
  {
    Q_ASSERT(via.protocol != NONE);
    Q_ASSERT(via.branch != "");
    Q_ASSERT(via.sentBy != "");

    SIPField field = {"Via", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

    field.valueSets[0].words.push_back("SIP/" + via.sipVersion +"/" + connectionToString(via.protocol));
    field.valueSets[0].words.push_back(via.sentBy + composePortString(via.port));

    if(!tryAddParameter(field.valueSets[0].parameters, "branch", via.branch))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }

    if(via.alias && !tryAddParameter(field.valueSets[0].parameters, "alias"))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }

    if(via.rport && !tryAddParameter(field.valueSets[0].parameters, "rport"))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }
    else if(via.rportValue !=0 && !tryAddParameter(field.valueSets[0].parameters,
                                                   "rport", QString::number(via.rportValue)))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }

    if (via.receivedAddress != "" && !tryAddParameter(field.valueSets[0].parameters,
                                                      "received", via.receivedAddress))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }

    fields.push_back(field);
  }
  return true;
}

bool includeMaxForwardsField(QList<SIPField> &fields,
                             std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message-> maxForwards != nullptr);
  Q_ASSERT(*message->maxForwards != 0);
  if (message-> maxForwards == nullptr || *message->maxForwards == 0)
  {
    printProgramError("SIPFieldComposing", "Failed to include Max-Forwards field");
    return false;
  }

  SIPField field = {"Max-Forwards",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(*message->maxForwards)}, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeContactField(QList<SIPField> &fields,
                         std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(!message->contact.empty());
  if (message->contact.empty())
  {
    return false;
  }

  SIPField field = {"Contact", QList<SIPValueSet>{}};

  for(auto& contact : message->contact)
  {
    Q_ASSERT(contact.address.uri.userinfo.user != "" &&
             contact.address.uri.hostport.host != "");
    if(contact.address.uri.userinfo.user == "" ||
       contact.address.uri.hostport.host == "")
    {
      printProgramError("SIPFieldComposing", "Failed to include Contact-field");
      return false;
    }

    contact.address.realname = "";
    contact.address.uri.uri_parameters.push_back({"transport", "tcp"});

    field.valueSets.push_back({{}, nullptr});

    if (!composeSIPRouteLocation(contact, field.valueSets.back()))
    {
      return false;
    }
  }

  fields.push_back(field);
  return true;
}

bool includeContentTypeField(QList<SIPField> &fields,
                             QString contentType)
{
  Q_ASSERT(contentType != "");
  if(contentType == "")
  {
    qDebug() << "WARNING: Content-type field failed";
    return false;
  }
  SIPField field = {"Content-Type",
                    QList<SIPValueSet>{SIPValueSet{{contentType}, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  SIPField field = {"Content-Length",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(contentLenght)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeExpiresField(QList<SIPField>& fields,
                         uint32_t expires)
{
  SIPField field = {"Expires",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(expires)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeRecordRouteField(QList<SIPField>& fields,
                             std::shared_ptr<SIPMessageHeader> message)
{
  SIPField field = {"Record-Route",{}};
  for (auto& route : message->recordRoutes)
  {
    field.valueSets.push_back({{}, nullptr});

    if (!composeSIPRouteLocation(route, field.valueSets.back()))
    {
      return false;
    }
  }
  fields.push_back(field);
  return true;
}


bool includeRouteField(QList<SIPField>& fields,
                       std::shared_ptr<SIPMessageHeader> message)
{
  SIPField field = {"Route",{}};
  for (auto& route : message->routes)
  {
    field.valueSets.push_back({{}, nullptr});

    if (!composeSIPRouteLocation(route, field.valueSets.back()))
    {
      return false;
    }
  }
  fields.push_back(field);
  return true;
}


bool includeAuthorizationField(QList<SIPField>& fields,
                               std::shared_ptr<SIPMessageHeader> message)
{
  Q_ASSERT(message->authorization != nullptr);
  Q_ASSERT(message->authorization->username != "");
  Q_ASSERT(message->authorization->realm != "");


  SIPField field = {"Authorization",
                    QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  field.valueSets[0].words.push_back("Digest");
  field.valueSets[0].words.push_back("username=\"" +
                                     message->authorization->username + "\"");
  field.valueSets[0].words.push_back("realm=\"" +
                                     message->authorization->realm + "\"");

  fields.push_back(field);

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
