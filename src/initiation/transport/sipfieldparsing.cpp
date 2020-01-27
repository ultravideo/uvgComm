#include "sipfieldparsing.h"

#include "sipconversions.h"

#include <QRegularExpression>
#include <QDebug>


// TODO: Support SIPS uri scheme. Needed for TLS
bool parseURI(QStringList &words, SIP_URI& uri);
ConnectionType parseUritype(QString type);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint(QString values, uint& number);


bool parseURI(QStringList& words, SIP_URI& uri)
{
  if (words.size() == 0 || words.size() > 2)
  {
    qDebug() << "PEER ERROR: Wrong amount of words in words list for URI. Expected 1-2. Got:"
             << words;
    return false;
  }

  int uriIndex = 0;
  if (words.size() == 2)
  {
    uri.realname = words.at(0);
    uriIndex = 1;
  }

  QRegularExpression re_field("<(\\w+):(?:(\\w+)@)?(.+)>");
  QRegularExpressionMatch field_match = re_field.match(words.at(uriIndex));

  // number of matches depends whether real name or the port were given
  if (field_match.hasMatch() &&
      field_match.lastCapturedIndex() == 3)
  {
    QString addressString = "";

    if(field_match.lastCapturedIndex() == 3)
    {
      uri.connectionType = parseUritype(field_match.captured(1));
      uri.username = field_match.captured(2);
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
          uri.parameters.push_back(SIPParameter());
          parseParameter(parameters.at(i), uri.parameters.back());
        }
      }

      QRegularExpression re_address("(\\[.+\\]|[\\w.]+):?(\\d*)");
      QRegularExpressionMatch address_match = re_address.match(parameters.first());

      if(address_match.hasMatch() &&
         address_match.lastCapturedIndex() >= 1 &&
         address_match.lastCapturedIndex() <= 2)
      {
        uri.host = address_match.captured(1);

        if (address_match.lastCapturedIndex() == 2 && address_match.captured(2) != "")
        {
          uri.port = address_match.captured(2).toUInt();
        }
        else
        {
          uri.port = 0;
        }
      }
      else
      {
        return false;
      }
    }

    return uri.connectionType != NONE;
  }

  return false;
}


ConnectionType parseUritype(QString type)
{
  if (type == "sip")
  {
    return TCP;
  }
  else if (type == "sips")
  {
    return TLS;
  }
  else if (type == "tel")
  {
    return TEL;
  }
  else
  {
    qDebug() << "ERROR:  Could not identify connection type:" << type;
  }

  return NONE;
}


bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value)
{
  if(parameters != nullptr)
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
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() >= 1 &&
      !parseURI(field.valueSets[0].words, message->to))
  {
    return false;
  }

  parseParameterNameToValue(field.valueSets[0].parameters, "tag", message->dialog->toTag);
  return true;
}


bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->dialog);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() >= 1 &&
      !parseURI(field.valueSets[0].words, message->from))
  {
    return false;
  }

  parseParameterNameToValue(field.valueSets[0].parameters, "tag", message->dialog->fromTag);
  return true;
}


bool parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() != 2)
  {
    return false;
  }

  bool ok = false;

  message->cSeq = field.valueSets[0].words[0].toUInt(&ok);
  message->transactionRequest = stringToRequest(field.valueSets[0].words[1]);
  return message->transactionRequest != SIP_NO_REQUEST && ok;
}


bool parseCallIDField(SIPField& field,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->dialog);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() != 1)
  {
    return false;
  }

  message->dialog->callID = field.valueSets[0].words[0];
  return true;
}


bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() != 2)
  {
    return false;
  }

  ViaInfo via = {NONE, "", "", 0, "", false, false, 0, ""};

  QRegularExpression re_first("SIP/(\\d.\\d)/(\\w+)");
  QRegularExpressionMatch first_match = re_first.match(field.valueSets[0].words[0]);

  if(!first_match.hasMatch() || first_match.lastCapturedIndex() != 2)
  {
    return false;
  }

  via.connectionType = stringToConnection(first_match.captured(2));
  via.version = first_match.captured(1);

  QRegularExpression re_second("([\\w.]+):?(\\d*)");
  QRegularExpressionMatch second_match = re_second.match(field.valueSets[0].words[1]);

  if(!second_match.hasMatch() || second_match.lastCapturedIndex() > 2)
  {
    return false;
  }

  via.address = second_match.captured(1);

  if (second_match.lastCapturedIndex() == 2)
  {
    via.port = second_match.captured(2).toUInt();
  }

  parseParameterNameToValue(field.valueSets[0].parameters, "branch", via.branch);
  parseParameterNameToValue(field.valueSets[0].parameters, "received", via.receivedAddress);

  QString rportValue = "";
  if (parseParameterNameToValue(field.valueSets[0].parameters, "rport", rportValue))
  {
    bool ok = false;
    via.rportValue = rportValue.toUInt(&ok);

    if (!ok)
    {
      via.rportValue = 0;
    }
    else
    {
      via.rport = true;
    }
  }

  message->vias.push_back(via);

  return true;
}


bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  return field.valueSets[0].words.size() == 1 &&
      parseUint(field.valueSets[0].words[0], message->maxForwards);
}


bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());
  return field.valueSets[0].words.size() == 1 &&
      parseURI(field.valueSets[0].words, message->contact);

  // TODO: parse expires parameter
}


bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  QRegularExpression re_field("(\\w+/\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.valueSets[0].words[0]);

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
  Q_ASSERT(!field.valueSets.empty());
  return field.valueSets[0].words.size() == 1 &&
      parseUint(field.valueSets[0].words[0], message->content.length);
}


bool parseRecordRouteField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());
  message->recordRoutes.push_back(SIP_URI{});
  return parseURI(field.valueSets[0].words, message->recordRoutes.back());
}


bool parseServerField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() < 1
      || field.valueSets[0].words.size() > 100)
  {
    return false;
  }

  message->server = field.valueSets[0].words[0];
  return true;
}


bool parseUserAgentField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  if (field.valueSets[0].words.size() < 1
      || field.valueSets[0].words.size() > 100)
  {
    return false;
  }

  message->userAgent = field.valueSets[0].words[0];
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
