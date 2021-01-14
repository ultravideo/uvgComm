#pragma once

#include "initiation/siptypes.h"



bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding);
bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding);

// The field functions only compose the field if the field data is present in
// the header!

// These functions work as follows: Create a field based on necessary info from
// the parameter and add the field to list. Later the fields are converted to string.

// returns whether the field was added

// NOTE: These functions mostly do not check validity of header pointer. Please make
// sure you provide a valid pointer!

bool includeAcceptField(QList<SIPField>& fields,
                        const std::shared_ptr<SIPMessageHeader> header);

bool includeAcceptEncodingField(QList<SIPField>& fields,
                                const std::shared_ptr<SIPMessageHeader> header);

bool includeAcceptLanguageField(QList<SIPField>& fields,
                                const std::shared_ptr<SIPMessageHeader> header);

bool includeAlertInfoField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header);

bool includeAllowField(QList<SIPField>& fields,
                       const std::shared_ptr<SIPMessageHeader> header);

bool includeAuthInfoField(QList<SIPField>& fields,
                          const std::shared_ptr<SIPMessageHeader> header);

bool includeAuthorizationField(QList<SIPField>& fields,
                               const std::shared_ptr<SIPMessageHeader> header);

// Always mandatory
bool includeCallIDField(QList<SIPField>& fields,
                        const std::shared_ptr<SIPMessageHeader> header);

bool includeCallInfoField(QList<SIPField>& fields,
                          const std::shared_ptr<SIPMessageHeader> header);

// Mandatory in INVITE request and INVITE OK response
bool includeContactField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header);

bool includeContentDispositionField(QList<SIPField>& fields,
                                    const std::shared_ptr<SIPMessageHeader> header);

bool includeContentEncodingField(QList<SIPField>& fields,
                                 const std::shared_ptr<SIPMessageHeader> header);

bool includeContentLanguageField(QList<SIPField>& fields,
                                 const std::shared_ptr<SIPMessageHeader> header);

bool includeContentLengthField(QList<SIPField>& fields,
                               const std::shared_ptr<SIPMessageHeader> header);

bool includeContentTypeField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header);

// Always mandatory
bool includeCSeqField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPMessageHeader> header);

bool includeDateField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPMessageHeader> header);

bool includeErrorInfoField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header);

bool includeExpiresField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header);

// Always mandatory
bool includeFromField(QList<SIPField>& fields,
                      const std::shared_ptr<SIPMessageHeader> header);

bool includeInReplyToField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header);

// Mandatory in requests, not allowed in responses
bool includeMaxForwardsField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header);

bool includeMinExpiresField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPMessageHeader> header);

bool includeMIMEVersionField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header);

bool includeOrganizationField(QList<SIPField>& fields,
                              const std::shared_ptr<SIPMessageHeader> header);

bool includePriorityField(QList<SIPField>& fields,
                          const std::shared_ptr<SIPMessageHeader> header);

bool includeProxyAuthenticateField(QList<SIPField>& fields,
                                   const std::shared_ptr<SIPMessageHeader> header);

bool includeProxyAuthorizationField(QList<SIPField>& fields,
                                    const std::shared_ptr<SIPMessageHeader> header);

bool includeProxyRequireField(QList<SIPField>& fields,
                              const std::shared_ptr<SIPMessageHeader> header);

bool includeRecordRouteField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header);

bool includeReplyToField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header);

bool includeRequireField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header);

bool includeRetryAfterField(QList<SIPField>& fields,
                            const std::shared_ptr<SIPMessageHeader> header);

bool includeRouteField(QList<SIPField>& fields,
                       const std::shared_ptr<SIPMessageHeader> header);

bool includeServerField(QList<SIPField>& fields,
                        const std::shared_ptr<SIPMessageHeader> header);

bool includeSubjectField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header);

bool includeSupportedField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header);

bool includeTimestampField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header);

// Always mandatory
bool includeToField(QList<SIPField>& fields,
                    const std::shared_ptr<SIPMessageHeader> header);

// only part of 420 response
bool includeUnsupportedField(QList<SIPField>& fields,
                             const std::shared_ptr<SIPMessageHeader> header);

bool includeUserAgentField(QList<SIPField>& fields,
                           const std::shared_ptr<SIPMessageHeader> header);

// Always mandatory
bool includeViaFields(QList<SIPField>& fields,
                      const std::shared_ptr<SIPMessageHeader> header);

bool includeWarningField(QList<SIPField>& fields,
                         const std::shared_ptr<SIPMessageHeader> header);

bool includeWWWAuthenticateField(QList<SIPField>& fields,
                                 const std::shared_ptr<SIPMessageHeader> header);
