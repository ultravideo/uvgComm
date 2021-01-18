#pragma once

#include "initiation/siptypes.h"


// a function used within this file to add a parameter
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName, QString parameterValue);
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName);

QString composeURItype(SIPType type);
QString composeSIPURI(const SIP_URI &uri);
QString composeAbsoluteURI(const AbsoluteURI&uri);
bool composeNameAddr(const NameAddr &nameAddr, QStringList &words);
QString composePortString(uint16_t port);

bool composeSIPRouteLocation(const SIPRouteLocation &location,
                             SIPCommaValue& value);

void composeDigestValue(QString fieldName, const QString& fieldValue, SIPField &field);
void composeDigestValueQuoted(QString fieldName, const QString& fieldValue, SIPField &field);


// composing whole fields
bool composeAcceptGenericField(QList<SIPField>& fields,
                          const std::shared_ptr<QList<SIPAcceptGeneric>> generics,
                          QString fieldname);

bool composeInfoField(QList<SIPField>& fields,
                      const QList<SIPInfo>& infos,
                      QString fieldname);

bool composeDigestChallengeField(QList<SIPField>& fields,
                                 const std::shared_ptr<QList<DigestChallenge>> dChallenge,
                                 QString fieldname);

bool composeDigestResponseField(QList<SIPField>& fields,
                                const std::shared_ptr<QList<DigestResponse>> dResponse,
                                QString fieldname);

bool composeStringList(QList<SIPField>& fields,
                            const QStringList& list,
                            QString fieldName);

bool composeString(QList<SIPField>& fields,
                   const QString& string,
                   QString fieldName);

