#pragma once

#include "initiation/siptypes.h"



bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding);
bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding);

// These functions work as follows: Create a field based on necessary info from
// the parameter and add the field to list. Later the fields are converted to string.

bool includeAcceptField(QList<SIPField>& fields,
                        const std::shared_ptr<QList<Accept>> accepts);

bool includeAcceptEncodingField(QList<SIPField>& fields,
                                const std::shared_ptr<QStringList> encodings);

bool includeAcceptLanguageField(QList<SIPField>& fields,
                                const std::shared_ptr<QStringList> languages);

bool includeAlertInfoField(QList<SIPField>& fields,
                           const QList<SIPInfo>& infos);

bool includeAllowField(QList<SIPField>& fields,
                       const std::shared_ptr<QList<SIPRequestMethod>> allows);

bool includeAuthInfoField(QList<SIPField>& fields,
                          const QList<SIPAuthInfo>& authInfos);

bool includeAuthorizationField(QList<SIPField>& fields,
                               const std::shared_ptr<DigestResponse> dResponse);

// Always mandatory
bool includeCallIDField(QList<SIPField>& fields,
                        const QString& callID);

bool includeCallInfoField(QList<SIPField>& fields,
                          const QList<SIPInfo>& infos);

// Mandatory in INVITE request and INVITE OK response
bool includeContactField(QList<SIPField>& fields,
                         const QList<SIPRouteLocation>& contacts);

bool includeContentDispositionField(QList<SIPField>& fields,
                                    const std::shared_ptr<ContentDisposition> disposition);

bool includeContentEncodingField(QList<SIPField>& fields,
                                 const QStringList& encodings);

bool includeContentLanguageField(QList<SIPField>& fields,
                                 const QStringList& languages);

bool includeContentLengthField(QList<SIPField>& fields, uint32_t contentLenght);

bool includeContentTypeField(QList<SIPField>& fields, QString contentType);

// Always mandatory
bool includeCSeqField(QList<SIPField>& fields,
                      const CSeqField& cSeq);

bool includeDateField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPDateField> date);

bool includeErrorInfoField(QList<SIPField>& fields,
                           const QList<SIPInfo>& infos);

bool includeExpiresField(QList<SIPField>& fields, uint32_t expires);

// Always mandatory
bool includeFromField(QList<SIPField>& fields,
                      const ToFrom& from);

bool includeInReplyToField(QList<SIPField>& fields,
                           const QString& callID);

// Mandatory in requests, not allowed in responses
bool includeMaxForwardsField(QList<SIPField>& fields,
                             const std::shared_ptr<uint8_t> maxForwards);

bool includeMinExpiresField(QList<SIPField>& fields,
                            std::shared_ptr<uint32_t> minExpires);

bool includeMIMEVersionField(QList<SIPField>& fields,
                             const QString& version);

bool includeOrganizationField(QList<SIPField>& fields,
                              const QString& organization);

bool includePriorityField(QList<SIPField>& fields,
                          const SIPPriorityField& priority);

bool includeProxyAuthenticateField(QList<SIPField>& fields,
                                   const std::shared_ptr<DigestChallenge> challenge);

bool includeProxyAuthorizationField(QList<SIPField>& fields,
                                    const std::shared_ptr<DigestResponse> dResponse);

bool includeProxyRequireField(QList<SIPField>& fields,
                              const QStringList& requires);

bool includeRecordRouteField(QList<SIPField>& fields,
                             const QList<SIPRouteLocation>& routes);

bool includeReplyToField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPRouteLocation> replyTo);

bool includeRequireField(QList<SIPField>& fields, const QStringList& requires);

bool includeRetryAfterField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPRetryAfter> retryAfter);

bool includeRouteField(QList<SIPField>& fields,
                       const QList<SIPRouteLocation>& routes);

bool includeServerField(QList<SIPField>& fields,
                        const QString& server);

bool includeSubjectField(QList<SIPField>& fields,
                         const QString& subject);

bool includeSupportedField(QList<SIPField>& fields,
                           const std::shared_ptr<QStringList> supported);

bool includeTimestampField(QList<SIPField>& fields, const QString& timestamp);

// Always mandatory
bool includeToField(QList<SIPField>& fields,
                    const ToFrom& to);

// only part of 420 response
bool includeUnsupportedField(QList<SIPField>& fields,
                             const QStringList& unsupported);

bool includeUserAgentField(QList<SIPField>& fields,
                           const QStringList& userAgents);

// Always mandatory
bool includeViaFields(QList<SIPField>& fields,
                      const QList<ViaField>& vias);

bool includeWarningField(QList<SIPField>& fields,
                         std::shared_ptr<SIPWarningField> warning);

bool includeWWWAuthenticateField(QList<SIPField>& fields,
                                 std::shared_ptr<DigestChallenge> challenge);
