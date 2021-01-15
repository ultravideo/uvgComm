#include "sipfieldcomposing.h"

#include "sipfieldcomposinghelper.h"
#include "sipconversions.h"

#include "common.h"



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

  if (request.method == SIP_REGISTER)
  {
    request.requestURI.userinfo.user = "";
    request.requestURI.userinfo.password = "";
  }

  request.requestURI.uri_parameters.push_back(SIPParameter{"transport", "tcp"});

  line = requestMethodToString(request.method) + " " +
      composeSIPURI(request.requestURI) +
      " SIP/" + request.sipVersion + lineEnding;

  return true;
}


bool getFirstResponseLine(QString& line, SIPResponse& response,
                          QString lineEnding)
{
  if(response.type == SIP_UNKNOWN_RESPONSE)
  {
    printProgramWarning("SIP Field Composing", "Found unknown response type.");
    return false;
  }
  line = "SIP/" + response.sipVersion + " "
      + QString::number(responseTypeToCode(response.type)) + " "
      + responseTypeToPhrase(response.type) + lineEnding;
  return true;
}


bool includeAcceptField(QList<SIPField>& fields,
                        const std::shared_ptr<SIPMessageHeader> header)
{
  if (header == nullptr ||
      header->accept == nullptr)
  {
    return false;
  }
  // empty list is also legal
  fields.push_back({"Accept",{}});

  for (auto& accept : *header->accept)
  {
    fields.back().valueSets.push_back({{contentTypeToString(accept.type)},{}});

    if (accept.parameter != nullptr)
    {
      if (!addParameter(fields.back().valueSets.back().parameters, *accept.parameter))
      {
        printProgramWarning("SIP Field Composing",
                            "Failed to add Accpet field parameter");
      }
    }
  }

  return true;
}


bool includeAcceptEncodingField(QList<SIPField>& fields,
                                const std::shared_ptr<SIPMessageHeader> header)
{
  return composeAcceptGenericField(fields, header->acceptEncoding, "Accept-Encoding");
}


bool includeAcceptLanguageField(QList<SIPField>& fields,
                                const std::shared_ptr<SIPMessageHeader> header)
{
  return composeAcceptGenericField(fields, header->acceptLanguage, "Accept-Language");
}


bool includeAlertInfoField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header)
{
  return composeInfoField(fields, header->alertInfos, "Alert-Info");
}


bool includeAllowField(QList<SIPField>& fields,
                       const std::shared_ptr<SIPMessageHeader> header)
{
  if (header == nullptr ||
      header->allow == nullptr)
  {
    return false;
  }

  // add field
  fields.push_back({"Allow",{}});

  for (auto& allow : *header->allow)
  {
    if (allow != SIP_NO_REQUEST)
    {
      // add comma(,) separated value. In this case one method.
      fields.back().valueSets.push_back({{requestMethodToString(allow)}, {}});
    }
  }

  return false;
}


bool includeAuthInfoField(QList<SIPField>& fields,
                          const std::shared_ptr<SIPMessageHeader> header)
{
  // if either field does not exist or none of the values have been set
  if (header == nullptr ||
      header->authInfo == nullptr ||
      (header->authInfo->nextNonce == "" &&
      header->authInfo->messageQop == SIP_NO_AUTH &&
      header->authInfo->responseAuth == "" &&
      header->authInfo->cnonce == "" &&
      header->authInfo->nonceCount == ""))
  {
    return false;
  }

  fields.push_back({"Allow",{}});

  // add each value as valueset if the value has been set
  composeDigestValueQuoted("nextnonce", header->authInfo->nextNonce, fields.back());
  composeDigestValue      ("qop",       qopValueToString(header->authInfo->messageQop), fields.back());
  composeDigestValueQuoted("rspauth",   header->authInfo->responseAuth, fields.back());
  composeDigestValueQuoted("cnonce",    header->authInfo->cnonce, fields.back());
  composeDigestValue      ("nc",        header->authInfo->nonceCount, fields.back());

  return true;
}


bool includeAuthorizationField(QList<SIPField>& fields,
                               const std::shared_ptr<SIPMessageHeader> header)
{
  return composeDigestResponseField(fields, header->authorization, "Authorization");
}


bool includeCallIDField(QList<SIPField> &fields,
                        const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->callID, "Call-ID");
}


bool includeCallInfoField(QList<SIPField>& fields,
                          const std::shared_ptr<SIPMessageHeader> header)
{
  return composeInfoField(fields, header->callInfos, "Call-Info");
}


bool includeContactField(QList<SIPField> &fields,
                         const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->contact.empty())
  {
    return false;
  }

  SIPField field = {"Contact", QList<SIPValueSet>{}};

  for(auto& contact : header->contact)
  {
    Q_ASSERT(contact.address.uri.userinfo.user != "" &&
             contact.address.uri.hostport.host != "");
    if(contact.address.uri.userinfo.user == "" ||
       contact.address.uri.hostport.host == "")
    {
      printProgramError("SIPFieldComposing", "Failed to include Contact-field");
      return false;
    }
    SIPRouteLocation modifiedContact = contact;
    modifiedContact.address.uri.uri_parameters.push_back({"transport", "tcp"});

    field.valueSets.push_back({{}, nullptr});

    if (!composeSIPRouteLocation(modifiedContact, field.valueSets.back()))
    {
      return false;
    }
  }

  fields.push_back(field);
  return true;
}


bool includeContentDispositionField(QList<SIPField>& fields,
                                    const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->contentDisposition == nullptr)
  {
    return false;
  }

  fields.push_back({"Content-Disposition", QList<SIPValueSet>{}});

  fields.back().valueSets.back().words.push_back(header->contentDisposition->dispType);

  for (auto& parameter : header->contentDisposition->parameters)
  {
    if (!addParameter(fields.back().valueSets.back().parameters, parameter))
    {
      printProgramWarning("SIP Field Composing",
                          "Faulty parameter in content-disposition");
    }
  }

  return true;
}


bool includeContentEncodingField(QList<SIPField>& fields,
                                 const std::shared_ptr<SIPMessageHeader> header)
{
  return composeStringList(fields, header->contentEncoding, "Content-Encoding");
}


bool includeContentLanguageField(QList<SIPField>& fields,
                                 const std::shared_ptr<SIPMessageHeader> header)
{
  return composeStringList(fields, header->contentLanguage, "Content-Language");
}


bool includeContentLengthField(QList<SIPField> &fields,
                               const std::shared_ptr<SIPMessageHeader> header)
{
  SIPField field = {"Content-Length",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(header->contentLength)},
                                                   nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeContentTypeField(QList<SIPField> &fields,
                             const std::shared_ptr<SIPMessageHeader> header)
{
  Q_ASSERT(header->contentType != MT_UNKNOWN);
  if(header->contentType == MT_UNKNOWN)
  {
    printProgramWarning("SIP Field Composing", "Content-type field failed.");
    return false;
  }

  if (header->contentType != MT_NONE)
  {
    SIPField field = {"Content-Type",
                      QList<SIPValueSet>{SIPValueSet{{contentTypeToString(header->contentType)},
                                                     nullptr}}};
    fields.push_back(field);
    return true;
  }

  return false; // type is not added if it is none
}


bool includeCSeqField(QList<SIPField> &fields,
                      const std::shared_ptr<SIPMessageHeader> header)
{
  Q_ASSERT(header->cSeq.cSeq != 0 && header->cSeq.method != SIP_NO_REQUEST);
  if (header->cSeq.cSeq == 0 || header->cSeq.method == SIP_NO_REQUEST)
  {
    printProgramWarning("SIP Field Composing", "CSeq field failed.");
    return false;
  }

  SIPField field = {"CSeq", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  field.valueSets[0].words.push_back(QString::number(header->cSeq.cSeq));
  field.valueSets[0].words.push_back(requestMethodToString(header->cSeq.method));
  field.valueSets[0].parameters = nullptr;

  fields.push_back(field);
  return true;
}


bool includeDateField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->date == nullptr ||
      header->date->weekday == "" ||
      header->date->date == "" ||
      header->date->time == "" ||
      header->date->timezone == "")
  {
    return false;
  }

  fields.push_back({"Date", QList<SIPValueSet>{SIPValueSet{}}});

  QString dateString = header->date->weekday + "," + " " + header->date->date + " " +
      header->date->time + " " + header->date->timezone;

  fields.back().valueSets.back().words.push_back(dateString);

  return true;
}


bool includeErrorInfoField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header)
{
  return composeInfoField(fields, header->errorInfos, "Error-Info");
}


bool includeExpiresField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->expires == nullptr)
  {
    return false;
  }

  SIPField field = {"Expires",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(*header->expires)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeFromField(QList<SIPField> &fields,
                      const std::shared_ptr<SIPMessageHeader> header)
{
  Q_ASSERT(header->from.address.uri.userinfo.user != "" &&
      header->from.address.uri.hostport.host != "");
  if(header->from.address.uri.userinfo.user == "" ||
     header->from.address.uri.hostport.host == "")
  {
    printProgramWarning("SIP Field Composing",
                        "Failed to compose From-field because of missing info",
                        "addressport", header->from.address.uri.userinfo.user +
                        "@" + header->from.address.uri.hostport.host);
    return false;
  }

  SIPField field = {"From", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  if (!composeNameAddr(header->from.address, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", header->from.tag);

  fields.push_back(field);
  return true;
}


bool includeInReplyToField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->inReplyToCallID, "In-Reply-To");
}


bool includeMaxForwardsField(QList<SIPField> &fields,
                             const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->maxForwards == nullptr || *header->maxForwards == 0)
  {
    return false;
  }

  return composeString(fields, QString::number(*header->maxForwards), "Max-Forwards");
}


bool includeMinExpiresField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->minExpires == nullptr ||
      *header->minExpires == 0)
  {
    return false;
  }

  return composeString(fields, QString::number(*header->minExpires), "Min-Expires");
}


bool includeMIMEVersionField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->mimeVersion, "MIME-Version");
}


bool includeOrganizationField(QList<SIPField>& fields,
                              const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->organization, "Organization");
}


bool includePriorityField(QList<SIPField>& fields,
                          const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, priorityToString(header->priority), "Priority");
}


bool includeProxyAuthenticateField(QList<SIPField>& fields,
                                   const std::shared_ptr<SIPMessageHeader> header)
{
  return composeDigestChallengeField(fields, header->proxyAuthenticate, "Proxy-Authenticate");
}


bool includeProxyAuthorizationField(QList<SIPField>& fields,
                                    const std::shared_ptr<SIPMessageHeader> header)
{
  return composeDigestResponseField(fields, header->proxyAuthorization, "Proxy-Authorization");
}


bool includeProxyRequireField(QList<SIPField>& fields,
                              const std::shared_ptr<SIPMessageHeader> header)
{
  return composeStringList(fields, header->proxyRequires, "Proxy-Require");
}


bool includeRecordRouteField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->recordRoutes.empty())
  {
    return false;
  }

  SIPField field = {"Record-Route",{}};
  for (auto& route : header->recordRoutes)
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


bool includeReplyToField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->replyTo == nullptr)
  {
    return false;
  }

  fields.push_back(SIPField{"Reply-To", QList<SIPValueSet>{SIPValueSet{},{}}});

  if (!composeSIPRouteLocation(*header->replyTo, fields.back().valueSets.back()))
  {
    fields.pop_back();
    return false;
  }


  return true;
}


bool includeRequireField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header)
{
  return composeStringList(fields, header->require, "Require");
}


bool includeRetryAfterField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->retryAfter != nullptr &&
      composeString(fields, QString::number(header->retryAfter->time), "Retry-After"))
  {
    if (!fields.empty() &&
        fields.back().name == "Retry-After" &&
        !fields.back().valueSets.empty())
    {
      if (header->retryAfter->duration != 0 &&
          !tryAddParameter(fields.back().valueSets.first().parameters,
                      "Duration", QString::number(header->retryAfter->duration)))
      {
        printProgramWarning("SIP Field Composing",
                            "Failed to add Retry-After duration parameter");
      }

      for (auto& parameter : header->retryAfter->parameters)
      {
        if (!addParameter(fields.back().valueSets.first().parameters, parameter))
        {
          printProgramWarning("SIP Field Composing",
                              "Failed to add Retry-After generic parameter");
        }
      }
    }


    return true;
  }
  return false;
}


bool includeRouteField(QList<SIPField>& fields,
                       const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->routes.empty())
  {
    return false;
  }

  SIPField field = {"Route",{}};
  for (auto& route : header->routes)
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


bool includeServerField(QList<SIPField>& fields,
                        const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->server, "Server");
}


bool includeSubjectField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->subject, "Subject");
}


bool includeSupportedField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->supported == nullptr)
  {
    return false;
  }
  return composeStringList(fields, *header->supported, "Supported");
}


bool includeTimestampField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->timestamp, "Timestamp");
}


bool includeToField(QList<SIPField> &fields,
                    const std::shared_ptr<SIPMessageHeader> header)
{
  Q_ASSERT(header->to.address.uri.userinfo.user != "" &&
      header->to.address.uri.hostport.host != "");
  if(header->to.address.uri.userinfo.user == "" ||
     header->to.address.uri.hostport.host == "")
  {
    printProgramWarning("SIP Field Composing",
                        "Failed to compose To-field because of missing",
                        "addressport", header->to.address.uri.userinfo.user +
                        "@" + header->to.address.uri.hostport.host);
    return false;
  }

  SIPField field = {"To", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  if (!composeNameAddr(header->to.address, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", header->to.tag);

  fields.push_back(field);
  return true;
}


bool includeUnsupportedField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header)
{
  return composeStringList(fields, header->unsupported, "Unsupported");
}


bool includeUserAgentField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header)
{
  return composeString(fields, header->userAgent, "User-Agent");
}


bool includeViaFields(QList<SIPField>& fields, const std::shared_ptr<SIPMessageHeader> header)
{
  Q_ASSERT(!header->vias.empty());
  if(header->vias.empty())
  {
    printProgramWarning("SIP Field Composing", "Via field failed.");
    return false;
  }

  for (ViaField& via : header->vias)
  {
    Q_ASSERT(via.protocol != NONE);
    Q_ASSERT(via.branch != "");
    Q_ASSERT(via.sentBy != "");

    SIPField field = {"Via", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

    field.valueSets[0].words.push_back("SIP/" + via.sipVersion +"/" +
                                       transportProtocolToString(via.protocol));
    field.valueSets[0].words.push_back(via.sentBy + composePortString(via.port));

    if(!tryAddParameter(field.valueSets[0].parameters, "branch", via.branch))
    {
      printProgramWarning("SIP Field Composing", "Via field branch failed.");
      return false;
    }

    if(via.alias && !tryAddParameter(field.valueSets[0].parameters, "alias"))
    {
      printProgramWarning("SIP Field Composing", "Via field alias failed.");
      return false;
    }

    if(via.rport && !tryAddParameter(field.valueSets[0].parameters, "rport"))
    {
      printProgramWarning("SIP Field Composing", "Via field rport failed.");
      return false;
    }
    else if(via.rportValue !=0 && !tryAddParameter(field.valueSets[0].parameters,
                                                   "rport", QString::number(via.rportValue)))
    {
      printProgramWarning("SIP Field Composing", "Via field rport value failed.");
      return false;
    }

    if (via.receivedAddress != "" && !tryAddParameter(field.valueSets[0].parameters,
                                                      "received", via.receivedAddress))
    {
      printProgramWarning("SIP Field Composing", "Via field receive address failed.");
      return false;
    }

    fields.push_back(field);
  }
  return true;
}


bool includeWarningField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header)
{
  if (header->warning.empty())
  {
    return false;
  }

  fields.push_back(SIPField{"Warning", QList<SIPValueSet>{}});

  for (auto& warning : header->warning)
  {
    fields.back().valueSets.push_back(SIPValueSet{});

    fields.back().valueSets.back().words.push_back(QString::number(warning.code));
    fields.back().valueSets.back().words.push_back(warning.warnAgent);
    fields.back().valueSets.back().words.push_back(warning.warnText);
  }

  return true;
}


bool includeWWWAuthenticateField(QList<SIPField>& fields,
                                 const std::shared_ptr<SIPMessageHeader> header)
{
  return composeDigestChallengeField(fields, header->wwwAuthenticate, "WWW-Authenticate");
}
