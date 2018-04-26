#include <sip/sipfieldparsing.h>

#include <sip/sipconversions.h>

#include <QRegularExpression>
#include <QDebug>


// TODO: Support SIPS uri scheme. Needed for TLS
bool parseURI(QString values, SIP_URI& uri);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint(QString values, uint& number);


bool parseURI(QString values, SIP_URI& uri)
{
  // RFC3261_TODO: Try to understand other than sip: addresses such as "tel:" and give error?
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
  Q_ASSERT(message->dialog);

  if(!parseURI(field.values, message->to))
  {
    return false;
  }

  parseParameterNameToValue(field.parameters, "tag", message->dialog->toTag);
  return true;
}

bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->dialog);

  if(!parseURI(field.values, message->from))
  {
    return false;
  }
  parseParameterNameToValue(field.parameters, "tag", message->dialog->fromTag);
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
  Q_ASSERT(message->dialog);

  QRegularExpression re_field("(\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
  {
    message->dialog->callID = field.values;
    return true;
  }

  return false;
}

bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  QRegularExpression re_field("SIP\/(\\d\.\\d)\/(\\w+) ([\\w\.:]+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  else if(field_match.lastCapturedIndex() == 3)
  {
    ViaInfo via = {stringToConnection(field_match.captured(2)),
                   field_match.captured(3),
                   field_match.captured(1), ""};

    parseParameterNameToValue(field.parameters, "branch", via.branch);
    message->senderReplyAddress.push_back(via);
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

  return parseUint(field.values, message->maxForwards);
}

bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  return parseURI(field.values, message->contact);
}

bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  QRegularExpression re_field("(\\w+/\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    message->content.type = stringToContentType(field_match.captured(1));
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
