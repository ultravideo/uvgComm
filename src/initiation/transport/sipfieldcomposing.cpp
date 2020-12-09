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

bool composeSIPUri(SIP_URI& uri, QStringList& words)
{
  if (uri.realname != "")
  {
    words.push_back("\"" + uri.realname + "\"");
  }

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


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding)
{
  if(request.requestURI.hostport.host == "")
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPComposing", 
               "Request URI host is empty when comprising the first line.");
  }

  if(request.type == SIP_NO_REQUEST)
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


  if(request.type != SIP_REGISTER)
  {
    type = composeUritype(request.requestURI.type);
    target = request.requestURI.userinfo.user + "@" + request.requestURI.hostport.host;
  }
  else // REGISTER first line does not contain username.
  {
    type = composeUritype(request.requestURI.type);
    target = request.requestURI.hostport.host;
  }

  line = requestToString(request.type) + " " + type
      + target + port + " SIP/" + request.message->version + lineEnding;

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
  line = "SIP/" + response.message->version + " "
      + QString::number(responseToCode(response.type)) + " "
      + responseToPhrase(response.type) + lineEnding;
  return true;
}

bool includeToField(QList<SIPField> &fields,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->to.userinfo.user != "" && message->to.hostport.host != "");
  if(message->to.userinfo.user == "" ||  message->to.hostport.host == "")
  {
    qDebug() << "WARNING: Composing To-field failed because host is:"
             << message->to.hostport.host << "and" << message->to.userinfo.user;
    return false;
  }

  SIPField field = {"To", QList<ValueSet>{ValueSet{{}, nullptr}}};

  if (!composeSIPUri(message->to, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", message->dialog->toTag);

  fields.push_back(field);
  return true;
}

bool includeFromField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->from.userinfo.user != "" && message->from.hostport.host != "");
  if(message->from.userinfo.user == "" ||  message->from.hostport.host == "")
  {
    qDebug() << "WARNING: From field failed";
    return false;
  }

  SIPField field = {"From", QList<ValueSet>{ValueSet{{}, nullptr}}};

  if (!composeSIPUri(message->from, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", message->dialog->fromTag);

  fields.push_back(field);
  return true;
}

bool includeCSeqField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->cSeq != 0 && message->transactionRequest != SIP_NO_REQUEST);
  if(message->cSeq == 0 || message->transactionRequest == SIP_NO_REQUEST)
  {
    qDebug() << "WARNING: cSeq field failed";
    return false;
  }

  SIPField field = {"CSeq", QList<ValueSet>{ValueSet{{}, nullptr}}};

  field.valueSets[0].words.push_back(QString::number(message->cSeq));
  field.valueSets[0].words.push_back(requestToString(message->transactionRequest));
  field.valueSets[0].parameters = nullptr;

  fields.push_back(field);
  return true;
}

bool includeCallIDField(QList<SIPField> &fields,
                        std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->dialog->callID != "");
  if(message->dialog->callID == "")
  {
    qDebug() << "WARNING: Call-ID field failed";
    return false;
  }

  SIPField field = {"Call-ID", QList<ValueSet>{ValueSet{{message->dialog->callID}, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeViaFields(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(!message->vias.empty());
  if(message->vias.empty())
  {
    qDebug() << "WARNING: Via field failed";
    return false;
  }

  for(ViaInfo via : message->vias)
  {
    Q_ASSERT(via.connectionType != NONE);
    Q_ASSERT(via.branch != "");
    Q_ASSERT(via.address != "");

    SIPField field = {"Via", QList<ValueSet>{ValueSet{{}, nullptr}}};

    field.valueSets[0].words.push_back("SIP/" + via.version +"/" + connectionToString(via.connectionType));
    field.valueSets[0].words.push_back(via.address + composePortString(via.port));

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
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->maxForwards != 0);
  if(message->maxForwards == 0)
  {
    qDebug() << "WARNING: Max-forwards field failed";
    return false;
  }

  SIPField field = {"Max-Forwards",
                    QList<ValueSet>{ValueSet{{QString::number(message->maxForwards)}, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeContactField(QList<SIPField> &fields,
                         std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->contact.userinfo.user != "" && message->contact.hostport.host != "");
  if(message->contact.userinfo.user == "" ||  message->contact.hostport.host == "")
  {
    qDebug() << "WARNING: Contact field failed";
    return false;
  }

  SIPField field = {"Contact", QList<ValueSet>{ValueSet{{}, nullptr}}};

  QString transportString = "";

  message->contact.realname = "";
  message->contact.uri_parameters.push_back({"transport", "tcp"});

  if (!composeSIPUri(message->contact, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

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
                    QList<ValueSet>{ValueSet{{contentType}, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  SIPField field = {"Content-Length",
                    QList<ValueSet>{ValueSet{{QString::number(contentLenght)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeExpiresField(QList<SIPField>& fields,
                         uint32_t expires)
{
  SIPField field = {"Expires",
                    QList<ValueSet>{ValueSet{{QString::number(expires)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeRecordRouteField(QList<SIPField>& fields,
                             std::shared_ptr<SIPMessageInfo> message)
{
  for (auto& record : message->recordRoutes)
  {
    SIPField field = {"Record-Route",
                      QList<ValueSet>{ValueSet{{}, nullptr}}};

    if (!composeSIPUri(record, field.valueSets[0].words))
    {
      return false;
    }
    fields.push_back(field);
  }
  return true;
}


bool includeRouteField(QList<SIPField>& fields,
                       std::shared_ptr<SIPMessageInfo> message)
{
  for (auto& route : message->routes)
  {
    SIPField field = {"Route",
                      QList<ValueSet>{ValueSet{{}, nullptr}}};

    if (!composeSIPUri(route, field.valueSets[0].words))
    {
      return false;
    }
    fields.push_back(field);
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
