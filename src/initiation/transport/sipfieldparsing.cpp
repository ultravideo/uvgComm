#include "sipfieldparsing.h"

#include "sipconversions.h"
#include "common.h"

#include <QRegularExpression>
#include <QDebug>


// TODO: Support SIPS uri scheme. Needed for TLS
bool parseURI(QStringList &words, SIP_URI& uri);
bool parseUritype(QString type, SIPType &out_Type);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint(QString values, uint& number);

bool checkAlwaysMandatoryFields(QList<SIPField>& fields);

bool alwaysSensibleField(QString field);
bool sensiblePreconditions(QString field, SIPRequestMethod method, QList<SIPField>& fields);


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

  // for example <sip:bob@biloxi.com>
  // ?: means it wont create a capture group
  // TODO: accept passwords
  QRegularExpression re_field("<(\\w+):(?:(\\w+)@)?(.+)>");
  QRegularExpressionMatch field_match = re_field.match(words.at(uriIndex));

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
    qDebug() << "ERROR:  Could not identify connection type:" << type;
    return false;
  }

  return true;
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
                  std::shared_ptr<SIPMessageBody> message)
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
                    std::shared_ptr<SIPMessageBody> message)
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
                  std::shared_ptr<SIPMessageBody> message)
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
                      std::shared_ptr<SIPMessageBody> message)
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
                   std::shared_ptr<SIPMessageBody> message)
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
  }

  message->vias.push_back(via);

  return true;
}


bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageBody> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  return field.valueSets[0].words.size() == 1 &&
      parseUint(field.valueSets[0].words[0], message->maxForwards);
}


bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageBody> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());
  return field.valueSets[0].words.size() == 1 &&
      parseURI(field.valueSets[0].words, message->contact);

  // TODO: parse expires parameter
}


bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageBody> message)
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
                             std::shared_ptr<SIPMessageBody> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());
  return field.valueSets[0].words.size() == 1 &&
      parseUint(field.valueSets[0].words[0], message->content.length);
}


bool parseRecordRouteField(SIPField& field,
                  std::shared_ptr<SIPMessageBody> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());
  message->recordRoutes.push_back(SIP_URI{});
  return parseURI(field.valueSets[0].words, message->recordRoutes.back());
}


bool parseServerField(SIPField& field,
                  std::shared_ptr<SIPMessageBody> message)
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
                  std::shared_ptr<SIPMessageBody> message)
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


bool parseUnimplemented(SIPField& field,
                      std::shared_ptr<SIPMessageBody> message)
{
  Q_ASSERT(message);
  Q_ASSERT(!field.valueSets.empty());

  printUnimplemented("SIPFieldParsing", "Found unsupported SIP field type: " + field.name);

  return false;
}

int countVias(QList<SIPField> &fields)
{
  int vias = 0;
  for(SIPField field : fields)
  {
    if(field.name == "Via")
    {
      ++vias;
    }
  }

  return vias;
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


bool checkRequestMustFields(QList<SIPField>& fields, SIPRequestMethod method)
{
  Q_ASSERT(method != SIP_REGISTER);
  Q_ASSERT(method != SIP_NO_REQUEST);
  Q_ASSERT(!fields.empty());

  if (!checkAlwaysMandatoryFields(fields))
  {
    printPeerError("SIPFieldParsing", "Request did not include "
                         "all universally mandatory fields!");
    return false;
  }

  // There are request header fields that are mandatory in certain situations,
  // but the RFC 3261 says we should be prepared to receive message
  // without them. That is the reason we don't fail because of them.

  // Contact is mandatory in INVITE
  if (method == SIP_INVITE)
  {
    if (!isLinePresent("Contact", fields))
    {
      printPeerError("SIPFieldParsing", "Received INVITE request without contact-field!");
      return false;
    }

    if (!isLinePresent("Supported", fields))
    {
      printWarning("SIPFieldParsing", "Received an INVITE Request "
                                      "without Supported-field, "
                                      "even though it should be included!");
    }
  }


  if (method == SIP_OPTIONS)
  {
    if (!isLinePresent("Accept", fields))
    {
      printWarning("SIPFieldParsing", "Received SIP OPTIONS request without Accept-field, "
                         "even though it should be included!");
    }
  }

  return true;
}


bool checkResponseMustFields(QList<SIPField>& fields, SIPResponseStatus status,
                             SIPRequestMethod ongoingTransaction)
{
  int code = responseToCode(status);

  if (!checkAlwaysMandatoryFields(fields))
  {
    printPeerError("SIPFieldParsing", "Response did not include "
                         "all universally mandatory fields!");
    return false;
  }

  if (code == 405)
  {
    if (!isLinePresent("Allow", fields))
    {
      printPeerError("SIPFieldParsing", "Received 405 Not allowed response without "
                       "allow field, even though it is mandatory!");

      return false;
    }
  }

  if (ongoingTransaction == SIP_INVITE &&
      code >= 200 && code <= 299)
  {
    if (!isLinePresent("Contact", fields))
    {
      printPeerError("SIPFieldParsing", "No contact-field in INVITE OK response!");
      return false;
    }
  }

  if (ongoingTransaction == SIP_REGISTER &&
      code == 423)
  {
    if (!isLinePresent("Min-Expires", fields))
    {
      printPeerError("SIPFieldParsing", "No Min-Expires-field in SIP 423 "
                           "Interval Too Brief response!");
      return false;
    }
  }

  if (code == 401)
  {
    if (!isLinePresent("WWW-Authenticate", fields))
    {
      printPeerError("SIPFieldParsing", "Received OK response to OPTIONS or INVITE request without "
                           "WWW-Authenticate field, even though it is mandatory!");
      return false;
    }
  }

  if (code == 420)
  {
    if (!isLinePresent("Unsupported", fields))
    {
      printPeerError("SIPFieldParsing", "Received 420 Bad Extension response without "
                           "Unsupported-field, even though it is mandatory!");
      return false;
    }
  }

  // mandatory, but we should still accept messages without these
  if (ongoingTransaction == SIP_OPTIONS &&
      (code >= 200 && code <= 299))
  {
    if (!isLinePresent("Accept", fields))
    {
      printWarning("SIPFieldParsing", "Received OK response to OPTIONS request without "
                         "Accept field, even though it should be included!");
    }

    if (!isLinePresent("Accept-Encoding", fields))
    {
      printWarning("SIPFieldParsing", "Received OK response to OPTIONS request without "
                         "Accept-Encoding field, even though it should be included!");
    }

    if (!isLinePresent("Accept-Language", fields))
    {
      printWarning("SIPFieldParsing", "Received OK response to OPTIONS request without "
                         "Accept-Language field, even though it should be included!");
    }
  }

  if ((ongoingTransaction == SIP_OPTIONS || ongoingTransaction == SIP_INVITE) &&
      (code >= 200 && code <= 299))
  {
    if (!isLinePresent("Allow", fields))
    {
      printWarning("SIPFieldParsing", "Received OK response to OPTIONS or INVITE request without "
                         "allow field, even though it should be included!");
    }

    if (!isLinePresent("Supported", fields))
    {
      printWarning("SIPFieldParsing", "Received OK response to OPTIONS or INVITE request without "
                         "Supported-field, even though it should be included!");
    }
  }

  return true;
}


bool sensibleRequestField(QList<SIPField>& fields, SIPRequestMethod method, QString field)
{
  Q_ASSERT(method != SIP_REGISTER);

  if (!sensiblePreconditions(field, method, fields) ||
      method == SIP_REGISTER)
  {
    return false;
  }

  // Start checking if it makes sense
  if (alwaysSensibleField(field))
  {
    return true;
  }

  // These make always sense in requests, but not responses
  if (field == "Authorization" &&
      field == "Max-Forwards" &&
      field == "Route" &&
      field == "Record-Route")
  {
    return true;
  }

  // these make sense everywhere but CANCEL requests
  if (method != SIP_CANCEL &&
      (field == "Content-Disposition" ||
       field == "Content-Encoding" ||
       field == "Content-Language" ||
       field == "Content-Type" ||
       field == "MIME-Version" ||
       field == "Proxy-Authorization"))
  {
    return true;
  }

  // these make sense everywhere but ACK requests
  if (method != SIP_ACK &&
      field == "Supported")
  {
    return true;
  }

  // everywhere but ACK and CANCEL
  if (method != SIP_ACK && method != SIP_CANCEL &&
      (field == "Accept" ||
       field == "Accept-Encoding" ||
       field == "Accept-Language" ||
       field == "Allow" ||
       field == "Proxy-Require" ||
       field == "Require" ||
       field == "Unsupported"))
  {
    return true;
  }

  // These are make sense in INVITE or OPTIONS
  if ((method == SIP_INVITE || method == SIP_OPTIONS) &&
      (field == "Call-Info" ||
       field == "Contact" ||
       field == "Organization"))
  {
    return true;
  }

  // INVITE only fields
  if (method == SIP_INVITE &&
      (field == "Alert-Info" &&
       field == "Expires" &&
       field == "In-Reply-To" &&
       field == "Priority" &&
       field == "Reply-To" &&
       field == "Subject"))
  {
    return true;
  }

  return false;
}


bool sensibleResponseField(QList<SIPField>& fields, SIPResponseStatus status,
                           SIPRequestMethod ongoingTransaction,
                           QString field)
{
  Q_ASSERT(status != SIP_UNKNOWN_RESPONSE);
  Q_ASSERT(ongoingTransaction != SIP_ACK); // There is no such thing as an ACK response

  if (!sensiblePreconditions(field, ongoingTransaction, fields) ||
      status == SIP_UNKNOWN_RESPONSE ||
      ongoingTransaction == SIP_ACK)
  {
    printProgramError("SIPFieldParsing",
                      "Response field sensibility check preconditions failed");
    return false;
  }

  if (alwaysSensibleField(field))
  {
    return true;
  }

  // These make always sense in response, but not request
  if (field == "Server" ||
      field == "Warning")
  {
    return true;
  }

  if (status >= 300 && status <= 699 &&
      field == "Error-Info")
  {
    return true;
  }

  if ((status == 404 || status == 413 || status == 480 || status == 486 ||
       status == 500 || status == 503 || status == 600 || status == 603) &&
      field == "Retry-after")
  {
    return true;
  }

  if (status >= 200 && status <= 299 &&
      field == "Supported")
  {
    return true;
  }

  if (status == 401 &&
      field == "Proxy-Authenticate")
  {
    return true;
  }

  // Not allowed in responses to REGISTER
  if (ongoingTransaction != SIP_REGISTER &&
      ((status >= 180 && status <= 189) || (status >= 200 && status <= 299)) &&
      field == "Record-Route")
  {
    return true;
  }

  // Not allowed in responses to CANCEL
  if (ongoingTransaction != SIP_CANCEL)
  {
    if (field == "Allow" ||
        field == "Content-Disposition" ||
        field == "Content-Encoding" ||
        field == "Content-Language" ||
        field == "Content-Type" ||
        field == "MIME-Version" ||
        field == "Require") // conditional, but we don't know the condition
    {
      return true;
    }

    // conditional, but we don't know the condition
    if (status == 415 &&
        (field == "Accept" ||
         field == "Accept-Encoding" ||
         field == "Accept-Language"))
    {
        return true;
    }

    if (status >= 200 && status <= 299 &&
        field == "Authentication-Info")
    {
      return true;
    }

    if (((status >= 300 && status <= 399) || status == 485) &&
        field == "Contact")
    {
      return true;
    }

    if ((status == 401 || status == 407) &&
        field == "WWW-Authenticate")
    {
      return true;
    }

    if (status == 407 &&
        field == "Proxy-Authenticate")
    {
      return true;
    }

    if (status == 420 &&
        field == "Unsupported")
    {
      return true;
    }
  }

  // Not allowed in responses to CANCEL or BYE
  if (ongoingTransaction != SIP_CANCEL && ongoingTransaction != SIP_BYE)
  {
    if (field == "Call-Info" ||
        field == "Organization")
    {
      return true;
    }

    if (status >= 200 && status <= 299 &&
        (field == "Accept" ||
         field == "Accept-Encoding" ||
         field == "Accept-Language" ||
         field == "Allow" ||
         field == "Contact" ||
         field == "Supported"))
    {
      return true;
    }
  }

  // Only with INVITE or REGISTER responses
  if ((ongoingTransaction == SIP_INVITE || ongoingTransaction == SIP_REGISTER) &&
      field == "Expires")
  {
    return true;
  }

  // Only with REGISTER responses
  if (ongoingTransaction == SIP_REGISTER &&
      status == 423 &&
      field == "Min-Expires")
  {
    return true;
  }

  // Only with INVITE responses
  if (ongoingTransaction == SIP_INVITE)
  {
    if (field == "Reply-To")
    {
      return true;
    }

    if (status == 180 &&
        field == "Alert-Info")
    {
      return true;
    }

    if (status >= 100 && status <= 199 &&
        field == "Contact")
    {
      return true;
    }
  }

  return false;
}


bool alwaysSensibleField(QString field)
{
  if (field == "Call-ID" ||
      field == "Content-Length" ||
      field == "CSeq" ||
      field == "Date" ||
      field == "From" ||
      field == "Timestamp" ||
      field == "To" ||
      field == "Via")
  {
    return true;
  }
  return false;
}


bool sensiblePreconditions(QString field, SIPRequestMethod method, QList<SIPField>& fields)
{
  // precondition checks

  Q_ASSERT(method != SIP_NO_REQUEST);
  Q_ASSERT(field != "");
  Q_ASSERT(!fields.empty());

  if (method == SIP_NO_REQUEST)
  {
    printProgramError("SIPFieldParsing",
                      "Bad request type parameter in sensibleRequestField");
    return false;
  }

  return true;
}


bool checkAlwaysMandatoryFields(QList<SIPField>& fields)
{
  // Not the fastest solution if there are many header fields,
  // but that shouldn't be an issue

  if (isLinePresent("Call-ID",        fields) &&
      isLinePresent("CSeq",           fields) &&
      isLinePresent("From",           fields) &&
      isLinePresent("Max-Forwards",   fields) &&
      isLinePresent("To",             fields) &&
      isLinePresent("Via",            fields) &&
      isLinePresent("Content-Length", fields)) // mandatory in TCP and TLS
  {
    return true;
  }

  return false;
}
