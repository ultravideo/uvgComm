#include "sipmessagesanity.h"

#include "sipfieldparsing.h"
#include "sipconversions.h"

#include "common.h"

bool checkAlwaysMandatoryFields(QList<SIPField>& fields);
bool alwaysSensibleField(QString field);
bool sensiblePreconditions(QString field, SIPRequestMethod method);

// check if the SIP message contains all required fields
bool checkRequestMustFields(QList<SIPField>& fields, SIPRequestMethod method);
bool checkResponseMustFields(QList<SIPField>& fields, SIPResponseStatus status,
                             SIPRequestMethod ongoingTransaction);





// tell whether a particular field is present in list.
bool isLinePresent(QString name, QList<SIPField> &fields);

int countVias(QList<SIPField> &fields);


bool isLinePresent(QString name, QList<SIPField>& fields)
{
  for(SIPField& field : fields)
  {
    if(field.name == name)
    {
      return true;
    }
  }
  printWarning("SIP Message Sanity", "Did not find field in header",
               "Name", name);
  return false;
}


int countVias(QList<SIPField> &fields)
{
  int vias = 0;
  for(SIPField& field : fields)
  {
    if(field.name == "Via")
    {
      ++vias;
    }
  }

  return vias;
}


bool requestSanityCheck(QList<SIPField>& fields, SIPRequestMethod method)
{

  printNormal("SIP Message Sanity",
              "SIP Request Sanity check starting. Checking presence of mandatory fields",
              "Method", requestMethodToString(method));

  // check if mandatory fields are present
  if (!checkRequestMustFields(fields, method))
  {
    return false;
  }

  printNormal("SIP Message Sanity",
              "SIP Request Sanity check ongoing. Scrubbing illegal fields.");

  // remove invalid fields
  for (int i = fields.size() - 1; i >= 0; --i)
  {
    if (!sensibleRequestField(method, fields.at(i).name))
    {
      fields.removeAt(i);
    }
  }

  printNormal("SIP Message Sanity",
              "SIP Request Sanity check over.");

  return true;
}


bool responseSanityCheck(QList<SIPField>& fields,
                                       SIPResponseStatus status)
{
  printNormal("SIP Message Sanity",
              "SIP Response Sanity check starting. Checking presence of mandatory fields",
              "Response Code", QString::number(responseTypeToCode(status)));

  if(status == SIP_UNKNOWN_RESPONSE)
  {
    printWarning("SIP Message Sanity", "Could not recognize SIP Response type!");
    return false;
  }

  SIPRequestMethod ongoingTransaction = SIP_NO_REQUEST;

  if (countVias(fields) > 1)
  {
    printError("SIP Message Sanity", "Too many Vias in SIP Response");
    return false;
  }

  for (auto& field : fields)
  {
    if (field.name == "CSeq")
    {
      std::shared_ptr<SIPMessageHeader> message
          = std::shared_ptr<SIPMessageHeader> (new SIPMessageHeader);
      if (parseCSeqField(field, message))
      {
        ongoingTransaction = message->cSeq.method;
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

  printNormal("SIP Message Sanity",
              "SIP Response Sanity check ongoing. Scrubbing illegal fields.");

  // remove invalid fields
  for (int i = fields.size() - 1; i >= 0; --i)
  {
    if (!sensibleResponseField(status, ongoingTransaction,
                               fields.at(i).name))
    {
      fields.removeAt(i);
    }
  }

  printNormal("SIP Message Sanity",
              "SIP Response Sanity check over.");

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

  if (!isLinePresent("Max-Forwards", fields))
  {
    printError("SIP Message Sanity", "SIP Request has no max-forwards field!");
  }

  // There are request header fields that are mandatory in certain situations,
  // but the RFC 3261 says we should be prepared to receive message
  // without them. That is the reason we don't fail because of them.

  // Contact is mandatory in INVITE
  if (method == SIP_INVITE)
  {
    if (!isLinePresent("Contact", fields))
    {
      printError("SIP Message Sanity", "INVITE Request has no contact-field!");
      return false;
    }

    if (!isLinePresent("Supported", fields))
    {
      printWarning("SIP Message Sanity", "An INVITE Request has no "
                      "Supported-field, even though it should be included!");
    }
  }


  if (method == SIP_OPTIONS)
  {
    if (!isLinePresent("Accept", fields))
    {
      printWarning("SIP Message Sanity", "SIP OPTIONS Request has no Accept-field, "
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
      printError("SIP Message Sanity", "405 Not Allowed Response has no "
                    "allow-field, even though it is mandatory!");

      return false;
    }
  }

  if (ongoingTransaction == SIP_INVITE &&
      status >= 200 && status <= 299)
  {
    if (!isLinePresent("Contact", fields))
    {
      printError("SIP Message Sanity", "No contact-field in INVITE OK response!");
      return false;
    }
  }

  if (ongoingTransaction == SIP_REGISTER &&
      status == 423)
  {
    if (!isLinePresent("Min-Expires", fields))
    {
      printError("SIP Message Sanity", "No Min-Expires-field in SIP 423 "
                    "Interval Too Brief response!");
      return false;
    }
  }

  if (status == 401)
  {
    if (!isLinePresent("WWW-Authenticate", fields))
    {
      printError("SIP Message Sanity", "401 Unauthorized Response to OPTIONS or INVITE request has no "
                    "WWW-Authenticate field, even though it is mandatory!");
      return false;
    }
  }

  if (status == 420)
  {
    if (!isLinePresent("Unsupported", fields))
    {
      printPeerError("SIP Message Sanity", "420 Bad Extension response has no "
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
      printWarning("SIP Message Sanity", "OK response to OPTIONS request has no "
                      "Accept-field, even though it should be included!");
    }

    if (!isLinePresent("Accept-Encoding", fields))
    {
      printWarning("SIP Message Sanity", "OK response to OPTIONS request has no "
                      "Accept-Encoding field, even though it should be included!");
    }

    if (!isLinePresent("Accept-Language", fields))
    {
      printWarning("SIP Message Sanity", "OK response to OPTIONS request has no "
                      "Accept-Language field, even though it should be included!");
    }
  }

  if ((ongoingTransaction == SIP_OPTIONS || ongoingTransaction == SIP_INVITE) &&
      (status >= 200 && status <= 299))
  {
    if (!isLinePresent("Allow", fields))
    {
      printWarning("SIP Message Sanity", "OK response to OPTIONS or INVITE request has no "
                      "allow-field, even though it should be included!");
    }

    if (!isLinePresent("Supported", fields))
    {
      printWarning("SIP Message Sanity", "OK response to OPTIONS or INVITE request has no "
                      "Supported-field, even though it should be included!");
    }
  }

  return true;
}


bool sensibleRequestField(SIPRequestMethod method, const QString field)
{
  Q_ASSERT(method != SIP_REGISTER);

  if (!sensiblePreconditions(field, method) ||
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
  if (field == "Authorization" ||
      field == "Max-Forwards" ||
      field == "Route" ||
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

  printWarning("SIP Message Sanity", "Nonsensical field found in SIP Request!",
               "Field name", field);

  return false;
}


bool sensibleResponseField(SIPResponseStatus status,
                           SIPRequestMethod ongoingTransaction,
                           QString field)
{
  Q_ASSERT(status != SIP_UNKNOWN_RESPONSE);
  Q_ASSERT(ongoingTransaction != SIP_ACK); // There is no such thing as an ACK response

  if (!sensiblePreconditions(field, ongoingTransaction) ||
      status == SIP_UNKNOWN_RESPONSE ||
      ongoingTransaction == SIP_ACK)
  {
    printProgramError("SIP Message Sanity",
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

  printWarning("SIP Message Sanity", "Nonsensical field found in SIP Response",
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


bool sensiblePreconditions(QString field, SIPRequestMethod method)
{
  // precondition checks

  Q_ASSERT(method != SIP_NO_REQUEST);
  Q_ASSERT(field != "");

  if (method == SIP_NO_REQUEST)
  {
    printProgramError("SIP Message Sanity",
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
      isLinePresent("To",             fields) &&
      isLinePresent("Via",            fields) &&
      isLinePresent("Content-Length", fields)) // mandatory in TCP and TLS
  {
    return true;
  }

  printPeerError("SIP Message Sanity",
                 "All mandatory fields not present in SIP message!");

  return false;
}
