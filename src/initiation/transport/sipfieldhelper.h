#pragma once

#include "initiation/siptypes.h"


// ========= Composing functions =========

// a function used within this file to add a parameter
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName, QString parameterValue);
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName);

bool addParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                  const SIPParameter &parameter);

QString composeURItype(SIPType type);
QString composeSIPURI(const SIP_URI &uri);
QString composeAbsoluteURI(const AbsoluteURI&uri);
bool composeNameAddr(const NameAddr &nameAddr, QStringList &words);
QString composePortString(uint16_t port);

bool composeSIPRouteLocation(const SIPRouteLocation &location,
                             SIPValueSet& valueSet);

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


// ========= Parsing functions =========

bool parseURI(const QString& word, SIP_URI& uri);
bool parseNameAddr(const QStringList &words, NameAddr& nameAddr);
bool parseSIPRouteLocation(const SIPValueSet &valueSet, SIPRouteLocation& location);

bool parseUritype(QString type, SIPType &out_Type);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint64(QString values, uint64_t& number);
bool parseUint8(QString values, uint8_t& number);


bool parsingPreChecks(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message,
                      bool emptyPossible = false);

// takes the parameter string (name=value) and parses it to SIPParameter
// used by parse functions.
bool parseParameter(QString text, SIPParameter& parameter);
