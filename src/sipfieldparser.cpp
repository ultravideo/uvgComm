#include <sipfieldparser.h>

#include <sipconversions.h>

#include <QRegularExpression>
#include <QDebug>


bool parseURI(QString values, SIP_URI& uri);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);

bool parseURI(QString values, SIP_URI& uri)
{
  QRegularExpression re_field("(\\w* )?<sip:(\\w+)@([\\w\.:]+)>");
  QRegularExpressionMatch field_match = re_field.match(values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  if(field_match.lastCapturedIndex() == 3)
  {
    uri.realname = "";
    uri.username = field_match.captured(1);
    uri.host = field_match.captured(2);
  }
  else if(field_match.lastCapturedIndex() == 4)
  {
    uri.realname = field_match.captured(1);
    uri.username = field_match.captured(2);
    uri.host = field_match.captured(3);
  }
  return true;
}

bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value)
{
  if(parameters != NULL)
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

bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  if(!parseURI(field.values, message->routing->to))
  {
    return false;
  }

  parseParameterNameToValue(field.parameters, "tag", message->session->remoteTag);
  return true;
}

bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  if(!parseURI(field.values, message->routing->from))
  {
    return false;
  }
  parseParameterNameToValue(field.parameters, "tag", message->session->localTag);
  return true;
}

bool parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  QRegularExpression re_field("(\d+) (\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  else if(field_match.lastCapturedIndex() == 3)
  {
    message->cSeq = field_match.captured(1).toUInt();
    message->transactionRequest = stringToRequest(field_match.captured(2));
  }
  else
  {
    return false;
  }
  return true;
}

bool parseCallIDField(SIPField& field,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);
  QRegularExpression re_field("(\w+)@([\w\.:]+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  else if(field_match.lastCapturedIndex() == 3)
  {
    message->session->callID = field.values;
  }
  else
  {
    return false;
  }

  return true;
}

bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  QRegularExpression re_field("SIP\/(\\d\.\\d)\/(\\w+) ([\\w\.:]+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  else if(field_match.lastCapturedIndex() == 3)
  {
    ConnectInstructions via;
    if(field_match.captured(2) == "UDP")
    {
      via.type = UDP;
    }
    else if(field_match.captured(2) == "TCP")
    {
      via.type = TCP;
    }
    else if(field_match.captured(2) == "TLS")
    {
      via.type = TLS;
    }
    else
    {
      qDebug() << "Unsupported connection protocol detected in Via.";
      return false;
    }

    via.version = field_match.captured(1).toUInt();
    via.address = field_match.captured(3);
    message->routing->senderReplyAddress.push_back(via);
  }
  else
  {
    return false;
  }
  return true;
}

bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  return true;
}

bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  return true;
}

bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  return true;
}

bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);
  Q_ASSERT(message->session);

  return true;
}

bool isLinePresent(QString name, QList<SIPField>& fields)
{
  for(SIPField field : fields)
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
  QRegularExpression re_parameter("([^=]+)=([^;]+)");
  QRegularExpressionMatch parameter_match = re_parameter.match(text);
  if(parameter_match.hasMatch() && parameter_match.lastCapturedIndex() == 2)
  {
    parameter.name = parameter_match.captured(1);
    parameter.value = parameter_match.captured(2);
    return true;
  }

  return false;
}

