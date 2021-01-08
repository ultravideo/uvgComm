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

  QString type = "";
  QString target = "";
  QString port = "";

  if (request.requestURI.hostport.port != 0)
  {
    port = ":" + QString::number(request.requestURI.hostport.port) + ";transport=tcp";
  }


  if(request.method != SIP_REGISTER)
  {
    type = composeUritype(request.requestURI.type);
    target = request.requestURI.userinfo.user + "@" + request.requestURI.hostport.host;
  }
  else // REGISTER first line does not contain username.
  {
    type = composeUritype(request.requestURI.type);
    target = request.requestURI.hostport.host;
  }

  line = requestToString(request.method) + " " + type
      + target + port + " SIP/" + request.sipVersion + lineEnding;

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
      + QString::number(responseToCode(response.type)) + " "
      + responseToPhrase(response.type) + lineEnding;
  return true;
}


bool includeAcceptField(QList<SIPField>& fields,
                        const std::shared_ptr<QList<Accept> > accepts)
{
  return false;
}


bool includeAcceptEncodingField(QList<SIPField>& fields,
                                const std::shared_ptr<QStringList> encodings)
{
  return false;
}


bool includeAcceptLanguageField(QList<SIPField>& fields,
                                const std::shared_ptr<QStringList> languages)
{
  return false;
}


bool includeAlertInfoField(QList<SIPField>& fields,
                           const QList<SIPInfo>& infos)
{
  return false;
}


bool includeAllowField(QList<SIPField>& fields,
                       const std::shared_ptr<QList<SIPRequestMethod>> allows)
{
  return false;
}


bool includeAuthInfoField(QList<SIPField>& fields,
                          const QList<SIPAuthInfo>& authInfos)
{
  return false;
}


bool includeAuthorizationField(QList<SIPField>& fields,
                               const std::shared_ptr<DigestResponse> dResponse)
{
  Q_ASSERT(dResponse != nullptr);
  Q_ASSERT(dResponse->username != "");
  Q_ASSERT(dResponse->realm != "");


  SIPField field = {"Authorization",
                    QList<SIPValueSet>{SIPValueSet{{}, nullptr}}};

  field.valueSets[0].words.push_back("Digest");
  field.valueSets[0].words.push_back("username=\"" +
                                     dResponse->username + "\"");
  field.valueSets[0].words.push_back("realm=\"" +
                                     dResponse->realm + "\"");

  fields.push_back(field);

  return true;
}



bool includeCallIDField(QList<SIPField> &fields,
                        const QString& callID)
{
  Q_ASSERT(callID != "");
  if(callID == "")
  {
    printProgramWarning("SIP Field Composing", "Call-ID field failed.");
    return false;
  }

  SIPField field = {"Call-ID", QList<SIPValueSet>{SIPValueSet{{callID}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeCallInfoField(QList<SIPField>& fields,
                          const QList<SIPInfo>& infos)
{
  return false;
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
  return false;
}


bool includeContentEncodingField(QList<SIPField>& fields,
                                 const QStringList &encodings)
{
  return false;
}


bool includeContentLanguageField(QList<SIPField>& fields,
                                 const QStringList& languages)
{
  return false;
}


bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  SIPField field = {"Content-Length",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(contentLenght)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeContentTypeField(QList<SIPField> &fields,
                             QString contentType)
{
  Q_ASSERT(contentType != "");
  if(contentType == "")
  {
    printProgramWarning("SIP Field Composing", "Content-type field failed.");
    return false;
  }
  SIPField field = {"Content-Type",
                    QList<SIPValueSet>{SIPValueSet{{contentType}, nullptr}}};
  fields.push_back(field);
  return true;
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
  field.valueSets[0].words.push_back(requestToString(cSeq.method));
  field.valueSets[0].parameters = nullptr;

  fields.push_back(field);
  return true;
}


bool includeDateField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPDateField> date)
{
  return false;
}


bool includeErrorInfoField(QList<SIPField>& fields,
                           const QList<SIPInfo>& infos)
{
  return false;
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
  return false;
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

  SIPField field = {"Max-Forwards",
                    QList<SIPValueSet>{SIPValueSet{{QString::number(*maxForwards)}, nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeMinExpiresField(QList<SIPField>& fields,
                            std::shared_ptr<uint32_t> minExpires)
{
  return false;
}


bool includeMIMEVersionField(QList<SIPField>& fields,
                             const QString& version)
{
  return false;
}


bool includeOrganizationField(QList<SIPField>& fields,
                              const QString& organization)
{
  return false;
}


bool includePriorityField(QList<SIPField>& fields,
                          const SIPPriorityField& priority)
{
  return false;
}


bool includeProxyAuthenticateField(QList<SIPField>& fields,
                                   const std::shared_ptr<DigestChallenge> challenge)
{
  return false;
}


bool includeProxyAuthorizationField(QList<SIPField>& fields,
                                    const std::shared_ptr<DigestResponse> dResponse)
{
  return false;
}


bool includeProxyRequireField(QList<SIPField>& fields,
                              const QStringList& requires)
{
  return false;
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
  return false;
}


bool includeRequireField(QList<SIPField>& fields,
                         const QStringList& requires)
{
  return false;
}


bool includeRetryAfterField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPRetryAfter> retryAfter)
{
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
  return false;
}


bool includeSubjectField(QList<SIPField>& fields,
                         const QString& subject)
{
  return false;
}


bool includeSupportedField(QList<SIPField>& fields,
                           const std::shared_ptr<QStringList> supported)
{
  return false;
}


bool includeTimestampField(QList<SIPField>& fields,
                           const QString& timestamp)
{
  return false;
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
  return false;
}


bool includeUserAgentField(QList<SIPField>& fields,
                           const QStringList& userAgents)
{
  return false;
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

    field.valueSets[0].words.push_back("SIP/" + via.sipVersion +"/" + connectionToString(via.protocol));
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
                         std::shared_ptr<SIPWarningField> warning)
{
  return false;
}


bool includeWWWAuthenticateField(QList<SIPField>& fields,
                                 std::shared_ptr<DigestChallenge> challenge)
{
  return false;
}
