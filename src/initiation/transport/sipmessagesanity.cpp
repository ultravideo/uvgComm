#include "sipmessagesanity.h"

#include "sipfieldparsing.h"

#include "common.h"

bool checkAlwaysMandatoryFields(QList<SIPField>& fields);
bool alwaysSensibleField(QString field);
bool sensiblePreconditions(QString field, SIPRequestMethod method, QList<SIPField>& fields);

// check if the SIP message contains all required fields
bool checkRequestMustFields(QList<SIPField>& fields, SIPRequestMethod method);
bool checkResponseMustFields(QList<SIPField>& fields, SIPResponseStatus status,
                             SIPRequestMethod ongoingTransaction);

// check if the field makes sense in this request/response.
// Fields that do not should be ignored.
bool sensibleRequestField(QList<SIPField>& fields, SIPRequestMethod method, QString field);
bool sensibleResponseField(QList<SIPField>& fields, SIPResponseStatus status,
                           SIPRequestMethod ongoingTransaction, QString field);



// tell whether a particular field is present in list.
bool isLinePresent(QString name, QList<SIPField> &fields);

int countVias(QList<SIPField> &fields);



bool requestSanityCheck(QList<SIPField>& fields, SIPRequestMethod method)
{
  // check if mandatory fields are present
  if (!checkRequestMustFields(fields, method))
  {
    return false;
  }

  // remove invalid fields
  for (int i = fields.size() - 1; i >= 0; --i)
  {
    if (!sensibleRequestField(fields, method, fields.at(i).name))
    {
      fields.removeAt(i);
    }
  }

  return true;
}


bool responseSanityCheck(QList<SIPField>& fields,
                                       SIPResponseStatus status)
{
  if(status == SIP_UNKNOWN_RESPONSE)
  {
    printWarning("SIPMessageSanity", "Could not recognize response type!");
    return false;
  }

  SIPRequestMethod ongoingTransaction = SIP_NO_REQUEST;

  if (countVias(fields) > 1)
  {
    printPeerError("SIPMessageSanity", "Too many Vias in received response");
    return false;
  }

  for (auto& field : fields)
  {
    if (field.name == "CSeq")
    {
      std::shared_ptr<SIPMessageBody> message
          = std::shared_ptr<SIPMessageBody> (new SIPMessageBody);
      if (parseCSeqField(field, message))
      {
        ongoingTransaction = message->transactionRequest;
      }
      else
      {
        return false;
      }
    }
  }

  // check if mandatory fields are present
  if (!checkResponseMustFields(fields, status, ongoingTransaction))
  {
    return false;
  }

  // remove invalid fields
  for (int i = fields.size() - 1; i >= 0; --i)
  {
    if (!sensibleResponseField(fields, status, ongoingTransaction,
                               fields.at(i).name))
    {
      fields.removeAt(i);
    }
  }

  return true;
}


bool checkRequestMustFields(QList<SIPField>& fields, SIPRequestMethod method)
{
  Q_ASSERT(method != SIP_REGISTER);
  Q_ASSERT(method != SIP_NO_REQUEST);
  Q_ASSERT(!fields.empty());

  if (!checkAlwaysMandatoryFields(fields))
  {
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
      printPeerError("SIPMessageSanity", "Received INVITE request without contact-field!");
      return false;
    }

    if (!isLinePresent("Supported", fields))
    {
      printWarning("SIPMessageSanity", "Received an INVITE Request "
                                      "without Supported-field, "
                                      "even though it should be included!");
    }
  }


  if (method == SIP_OPTIONS)
  {
    if (!isLinePresent("Accept", fields))
    {
      printWarning("SIPMessageSanity", "Received SIP OPTIONS request without Accept-field, "
                         "even though it should be included!");
    }
  }

  return true;
}


bool checkResponseMustFields(QList<SIPField>& fields, SIPResponseStatus status,
                             SIPRequestMethod ongoingTransaction)
{
  if (!checkAlwaysMandatoryFields(fields))
  {
    return false;
  }

  if (status == 405)
  {
    if (!isLinePresent("Allow", fields))
    {
      printPeerError("SIPMessageSanity", "Received 405 Not allowed response without "
                       "allow field, even though it is mandatory!");

      return false;
    }
  }

  if (ongoingTransaction == SIP_INVITE &&
      status >= 200 && status <= 299)
  {
    if (!isLinePresent("Contact", fields))
    {
      printPeerError("SIPMessageSanity", "No contact-field in INVITE OK response!");
      return false;
    }
  }

  if (ongoingTransaction == SIP_REGISTER &&
      status == 423)
  {
    if (!isLinePresent("Min-Expires", fields))
    {
      printPeerError("SIPMessageSanity", "No Min-Expires-field in SIP 423 "
                           "Interval Too Brief response!");
      return false;
    }
  }

  if (status == 401)
  {
    if (!isLinePresent("WWW-Authenticate", fields))
    {
      printPeerError("SIPFieldParsing", "Received OK response to OPTIONS or INVITE request without "
                           "WWW-Authenticate field, even though it is mandatory!");
      return false;
    }
  }

  if (status == 420)
  {
    if (!isLinePresent("Unsupported", fields))
    {
      printPeerError("SIPMessageSanity", "Received 420 Bad Extension response without "
                           "Unsupported-field, even though it is mandatory!");
      return false;
    }
  }

  // mandatory, but we should still accept messages without these
  if (ongoingTransaction == SIP_OPTIONS &&
      (status >= 200 && status <= 299))
  {
    if (!isLinePresent("Accept", fields))
    {
      printWarning("SIPMessageSanity", "Received OK response to OPTIONS request without "
                         "Accept field, even though it should be included!");
    }

    if (!isLinePresent("Accept-Encoding", fields))
    {
      printWarning("SIPMessageSanity", "Received OK response to OPTIONS request without "
                         "Accept-Encoding field, even though it should be included!");
    }

    if (!isLinePresent("Accept-Language", fields))
    {
      printWarning("SIPMessageSanity", "Received OK response to OPTIONS request without "
                         "Accept-Language field, even though it should be included!");
    }
  }

  if ((ongoingTransaction == SIP_OPTIONS || ongoingTransaction == SIP_INVITE) &&
      (status >= 200 && status <= 299))
  {
    if (!isLinePresent("Allow", fields))
    {
      printWarning("SIPMessageSanity", "Received OK response to OPTIONS or INVITE request without "
                         "allow field, even though it should be included!");
    }

    if (!isLinePresent("Supported", fields))
    {
      printWarning("SIPMessageSanity", "Received OK response to OPTIONS or INVITE request without "
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

  printPeerError("SIPMessageSanity", "Nonsensical field found in request!",
                 "Field name", field);

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
    printProgramError("SIPMessageSanity",
                      "Response field sensibility check preconditions failed!");
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

  printPeerError("SIPMessageSanity", "Nonsensical field found in response",
                 "Field name", field);

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
    printProgramError("SIPMessageSanity",
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
    printPeerError("SIPMessageSanity",
                   "All mandatory fields not present in SIP message!");
    return true;
  }

  return false;
}
