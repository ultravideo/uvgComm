#include "sipfieldcomposing.h"

#include "sipfieldhelper.h"
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
                        const std::shared_ptr<QList<SIPAccept>> accepts)
{
  if (accepts == nullptr)
  {
    return false;
  }
  // empty list is also legal
  fields.push_back({"Accept",{}});

  for (auto& accept : *accepts)
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
                                const std::shared_ptr<QList<SIPAcceptGeneric>> encodings)
{
  return composeAcceptGenericField(fields, encodings, "Accept-Encoding");
}


bool includeAcceptLanguageField(QList<SIPField>& fields,
                                const std::shared_ptr<QList<SIPAcceptGeneric> > languages)
{
  return composeAcceptGenericField(fields, languages, "Accept-Language");
}


bool includeAlertInfoField(QList<SIPField>& fields,
                           const QList<SIPInfo>& infos)
{
  return composeInfoField(fields, infos, "Alert-Info");
}


bool includeAllowField(QList<SIPField>& fields,
                       const std::shared_ptr<QList<SIPRequestMethod>> allows)
{
  if (allows == nullptr)
  {
    return false;
  }

  // add field
  fields.push_back({"Allow",{}});

  for (auto& allow : *allows)
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
                          const std::shared_ptr<SIPAuthInfo> authInfo)
{
  // if either field does not exist or none of the values have been set
  if (authInfo == nullptr ||
      (authInfo->nextNonce == "" &&
      authInfo->messageQop == SIP_NO_AUTH &&
      authInfo->responseAuth == "" &&
      authInfo->cnonce == "" &&
      authInfo->nonceCount == ""))
  {
    return false;
  }

  fields.push_back({"Allow",{}});

  // add each value as valueset if the value has been set
  composeDigestValueQuoted("nextnonce", authInfo->nextNonce, fields.back());
  composeDigestValue      ("qop",       qopValueToString(authInfo->messageQop), fields.back());
  composeDigestValueQuoted("rspauth",   authInfo->responseAuth, fields.back());
  composeDigestValueQuoted("cnonce",    authInfo->cnonce, fields.back());
  composeDigestValue      ("nc",        authInfo->nonceCount, fields.back());

  return true;
}


bool includeAuthorizationField(QList<SIPField>& fields,
                               const std::shared_ptr<DigestResponse> dResponse)
{
  return composeDigestResponseField(fields, dResponse, "Authorization");
}


bool includeCallIDField(QList<SIPField> &fields,
                        const QString& callID)
{
  return composeString(fields, callID, "Call-ID");
}


bool includeCallInfoField(QList<SIPField>& fields,
                          const QList<SIPInfo>& infos)
{
  return composeInfoField(fields, infos, "Call-Info");
}


bool includeContactField(QList<SIPField> &fields,
                         const QList<SIPRouteLocation> &contacts)
{
  Q_ASSERT(!contacts.empty());
  if (contacts.empty())
  {
    return false;
  }

  SIPField field = {"Contact", QList<SIPValueSet>{}};

  for(auto& contact : contacts)
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
                                    const std::shared_ptr<ContentDisposition> disposition)
{
  if (disposition == nullptr)
  {
    return false;
  }

  fields.push_back({"Content-Disposition", QList<SIPValueSet>{}});

  fields.back().valueSets.back().words.push_back(disposition->dispType);

  for (auto& parameter : disposition->parameters)
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
                                 const QStringList &encodings)
{
  return composeCommaStringList(fields, encodings, "Content-Encoding");
}


bool includeContentLanguageField(QList<SIPField>& fields,
                                 const QStringList& languages)
{
  return composeCommaStringList(fields, languages, "Content-Language");
}


bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  SIPField field = {"Content-Length",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(contentLenght)},
                                                   nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeContentTypeField(QList<SIPField> &fields,
                             MediaType contentType)
{
  Q_ASSERT(contentType != MT_UNKNOWN);
  if(contentType == MT_UNKNOWN)
  {
    printProgramWarning("SIP Field Composing", "Content-type field failed.");
    return false;
  }

  if (contentType != MT_NONE)
  {
    SIPField field = {"Content-Type",
                      QList<SIPValueSet>{SIPValueSet{{contentTypeToString(contentType)},
                                                     nullptr}}};
    fields.push_back(field);
    return true;
  }

  return false; // type is not added if it is none
}


bool includeCSeqField(QList<SIPField> &fields,
                      const CSeqField &cSeq)
{
  Q_ASSERT(cSeq.cSeq != 0 && cSeq.method != SIP_NO_REQUEST);
  if (cSeq.cSeq == 0 || cSeq.method == SIP_NO_REQUEST)
  {
    printProgramWarning("SIP Field Composing", "CSeq field failed.");
    return false;
  }

  SIPField field = {"CSeq", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  field.valueSets[0].words.push_back(QString::number(cSeq.cSeq));
  field.valueSets[0].words.push_back(requestMethodToString(cSeq.method));
  field.valueSets[0].parameters = nullptr;

  fields.push_back(field);
  return true;
}


bool includeDateField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPDateField> date)
{
  if (date == nullptr ||
      date->weekday == "" ||
      date->date == "" ||
      date->time == "" ||
      date->timezone == "")
  {
    return false;
  }

  fields.push_back({"Date", QList<SIPValueSet>{SIPValueSet{}}});

  QString dateString = date->weekday + "," + " " + date->date + " " +
      date->time + " " + date->timezone;

  fields.back().valueSets.back().words.push_back(dateString);

  return true;
}


bool includeErrorInfoField(QList<SIPField>& fields,
                           const QList<SIPInfo>& infos)
{
  return composeInfoField(fields, infos, "Error-Info");
}


bool includeExpiresField(QList<SIPField>& fields,
                         uint32_t expires)
{
  SIPField field = {"Expires",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(expires)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeFromField(QList<SIPField> &fields,
                      const ToFrom &from)
{
  Q_ASSERT(from.address.uri.userinfo.user != "" &&
      from.address.uri.hostport.host != "");
  if(from.address.uri.userinfo.user == "" ||
     from.address.uri.hostport.host == "")
  {
    printProgramWarning("SIP Field Composing",
                        "Failed to compose From-field because of missing info",
                        "addressport", from.address.uri.userinfo.user +
                        "@" + from.address.uri.hostport.host);
    return false;
  }

  SIPField field = {"From", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  if (!composeNameAddr(from.address, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", from.tag);

  fields.push_back(field);
  return true;
}


bool includeInReplyToField(QList<SIPField>& fields,
                           const QString& callID)
{
  return composeString(fields, callID, "In-Reply-To");
}


bool includeMaxForwardsField(QList<SIPField> &fields,
                             const std::shared_ptr<uint8_t> maxForwards)
{
  Q_ASSERT(maxForwards != nullptr);
  Q_ASSERT(*maxForwards != 0);
  if (maxForwards == nullptr || *maxForwards == 0)
  {
    printProgramError("SIPFieldComposing", "Failed to include Max-Forwards field");
    return false;
  }

  return composeString(fields, QString::number(*maxForwards), "Max-Forwards");
}


bool includeMinExpiresField(QList<SIPField>& fields,
                            std::shared_ptr<uint32_t> minExpires)
{
  if (minExpires == nullptr || *minExpires == 0)
  {
    printProgramError("SIPFieldComposing", "Failed to include Min-Expires field");
    return false;
  }

  return composeString(fields, QString::number(*minExpires), "Min-Expires");
}


bool includeMIMEVersionField(QList<SIPField>& fields,
                             const QString& version)
{
  return composeString(fields, version, "MIME-Version");
}


bool includeOrganizationField(QList<SIPField>& fields,
                              const QString& organization)
{
  return composeString(fields, organization, "Organization");
}


bool includePriorityField(QList<SIPField>& fields,
                          const SIPPriorityField& priority)
{
  return composeString(fields, priorityToString(priority), "Priority");
}


bool includeProxyAuthenticateField(QList<SIPField>& fields,
                                   const std::shared_ptr<DigestChallenge> challenge)
{
  return composeDigestChallengeField(fields, challenge, "Proxy-Authenticate");
}


bool includeProxyAuthorizationField(QList<SIPField>& fields,
                                    const std::shared_ptr<DigestResponse> dResponse)
{
  return composeDigestResponseField(fields, dResponse, "Proxy-Authorization");
}


bool includeProxyRequireField(QList<SIPField>& fields,
                              const QStringList& requires)
{
  return composeCommaStringList(fields, requires, "Proxy-Require");
}


bool includeRecordRouteField(QList<SIPField>& fields,
                             const QList<SIPRouteLocation> &routes)
{
  if (routes.empty())
  {
    return false;
  }

  SIPField field = {"Record-Route",{}};
  for (auto& route : routes)
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
                         const std::shared_ptr<SIPRouteLocation> replyTo)
{
  if (replyTo == nullptr)
  {
    return false;
  }

  fields.push_back(SIPField{"Reply-To", QList<SIPValueSet>{SIPValueSet{},{}}});

  if (!composeSIPRouteLocation(*replyTo, fields.back().valueSets.back()))
  {
    fields.pop_back();
    return false;
  }


  return true;
}


bool includeRequireField(QList<SIPField>& fields,
                         const QStringList& requires)
{
  return composeCommaStringList(fields, requires, "Require");
}


bool includeRetryAfterField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPRetryAfter> retryAfter)
{
  if (retryAfter != nullptr &&
      composeString(fields, QString::number(retryAfter->time), "Retry-After"))
  {
    if (!fields.empty() &&
        fields.back().name == "Retry-After" &&
        !fields.back().valueSets.empty())
    {
      if (retryAfter->duration != 0 &&
          !tryAddParameter(fields.back().valueSets.first().parameters,
                      "Duration", QString::number(retryAfter->duration)))
      {
        printProgramWarning("SIP Field Composing",
                            "Failed to add Retry-After duration parameter");
      }

      for (auto& parameter : retryAfter->parameters)
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
                       const QList<SIPRouteLocation> &routes)
{
  if (routes.empty())
  {
    return false;
  }

  SIPField field = {"Route",{}};
  for (auto& route : routes)
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
                        const QString& server)
{
  return composeString(fields, server, "Server");
}


bool includeSubjectField(QList<SIPField>& fields,
                         const QString& subject)
{
  return composeString(fields, subject, "Subject");
}


bool includeSupportedField(QList<SIPField>& fields,
                           const std::shared_ptr<QStringList> supported)
{
  if (supported == nullptr)
  {
    return false;
  }
  return composeCommaStringList(fields, *supported, "Supported");
}


bool includeTimestampField(QList<SIPField>& fields,
                           const QString& timestamp)
{
  return composeString(fields, timestamp, "Timestamp");
}


bool includeToField(QList<SIPField> &fields,
                    const ToFrom &to)
{
  Q_ASSERT(to.address.uri.userinfo.user != "" &&
      to.address.uri.hostport.host != "");
  if(to.address.uri.userinfo.user == "" ||
     to.address.uri.hostport.host == "")
  {
    printProgramWarning("SIP Field Composing",
                        "Failed to compose To-field because of missing",
                        "addressport", to.address.uri.userinfo.user +
                        "@" + to.address.uri.hostport.host);
    return false;
  }

  SIPField field = {"To", QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  if (!composeNameAddr(to.address, field.valueSets[0].words))
  {
    return false;
  }

  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", to.tag);

  fields.push_back(field);
  return true;
}


bool includeUnsupportedField(QList<SIPField>& fields,
                             const QStringList& unsupported)
{
  return composeCommaStringList(fields, unsupported, "Unsupported");
}


bool includeUserAgentField(QList<SIPField>& fields,
                           const QString& userAgent)
{
  return composeString(fields, userAgent, "User-Agent");
}


bool includeViaFields(QList<SIPField>& fields, const QList<ViaField>& vias)
{
  Q_ASSERT(!vias.empty());
  if(vias.empty())
  {
    printProgramWarning("SIP Field Composing", "Via field failed.");
    return false;
  }

  for(const ViaField& via : vias)
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
                         QList<SIPWarningField> warnings)
{
  if (warnings.empty())
  {
    return false;
  }

  fields.push_back(SIPField{"Warning", QList<SIPValueSet>{}});

  for (auto& warning : warnings)
  {
    fields.back().valueSets.push_back(SIPValueSet{});

    fields.back().valueSets.back().words.push_back(QString::number(warning.code));
    fields.back().valueSets.back().words.push_back(warning.warnAgent);
    fields.back().valueSets.back().words.push_back(warning.warnText);
  }

  return true;
}


bool includeWWWAuthenticateField(QList<SIPField>& fields,
                                 std::shared_ptr<DigestChallenge> challenge)
{
  return composeDigestChallengeField(fields, challenge, "WWW-Authenticate");
}
