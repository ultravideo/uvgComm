#include <sipfieldparser.h>

#include <sipconversions.h>

#include <QRegularExpression>
#include <QDebug>


bool parseURI(QString values, SIP_URI& uri);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint(QString values, uint& number);


bool parseURI(QString values, SIP_URI& uri)
{
  QRegularExpression re_field("(\\w+ )?<sip:(\\w+)@([\\w\.:]+)>");
  QRegularExpressionMatch field_match = re_field.match(values);

  if(field_match.hasMatch() && (field_match.lastCapturedIndex() == 4
          || field_match.lastCapturedIndex() == 3))
  {
    uri.realname = field_match.captured(1);
    uri.username = field_match.captured(2);
    uri.host = field_match.captured(3);
    return true;
  }

  return false;
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

bool parseUint(QString values, uint& number)
{
  QRegularExpression re_field("(\\d+)");
  QRegularExpressionMatch field_match = re_field.match(values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    number = values.toUInt();
    return true;
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

  QRegularExpression re_field("(\\d+) (\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
  {
    message->cSeq = field_match.captured(1).toUInt();
    message->transactionRequest = stringToRequest(field_match.captured(2));
    return true;
  }
  return false;
}

bool parseCallIDField(SIPField& field,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->session);
  QRegularExpression re_field("(\\w+)@([\\w\.:]+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
  {
    message->session->callID = field.values;
    return true;
  }

  return false;
}

bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);

  QRegularExpression re_field("SIP\/(\\d\.\\d)\/(\\w+) ([\\w\.:]+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  else if(field_match.lastCapturedIndex() == 3)
  {
    ViaInfo via;
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
      qDebug() << "Unrecognized connection protocol found in Via.";
      return false;
    }

    via.version = field_match.captured(1);
    via.address = field_match.captured(3);

    parseParameterNameToValue(field.parameters, "branch", via.branch);

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

  return parseUint(field.values, message->routing->maxForwards);
}

bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->routing);

  return parseURI(field.values, message->routing->contact);
}

bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  QRegularExpression re_field("(\\w+/\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    message->content.type = field_match.captured(1);
    return true;
  }
  return false;
}

bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  return parseUint(field.values, message->content.length);
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

