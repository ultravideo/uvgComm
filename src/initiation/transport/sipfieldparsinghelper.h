#pragma once

#include "initiation/siptypes.h"


bool parseURI(const QString& word, SIP_URI& uri);
bool parseNameAddr(const QStringList &words, NameAddr& nameAddr);
bool parseSIPRouteLocation(const SIPCommaValue &value, SIPRouteLocation& location);

bool parseUritype(QString type, SIPType &out_Type);
bool parseParameterByName(QList<SIPParameter> parameters,
                          QString name, QString& value);
bool parseUint64(QString values, uint64_t& number);
bool parseUint8(QString values, uint8_t& number);




// takes the parameter string (name=value) and parses it to SIPParameter
// used by parse functions.
bool parseParameter(QString text, SIPParameter& parameter);


bool parseAcceptGeneric(SIPField& field,
                        std::shared_ptr<QList<SIPAcceptGeneric>> generics);

bool parseDigestChallengeField(SIPField& field,
                               const std::shared_ptr<QList<DigestChallenge>> dChallenge);

bool parsaeDigestResponseField(QList<SIPField>& fields,
                               const std::shared_ptr<QList<DigestResponse>> dResponse,
                               QString fieldname);

