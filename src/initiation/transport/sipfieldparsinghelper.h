#pragma once

#include "initiation/siptypes.h"

#include <map>

bool parseURI(const QString& word, SIP_URI& uri);
bool parseAbsoluteURI(const QString& word, AbsoluteURI& uri);
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

bool parseInfo(SIPField& field,
               QList<SIPInfo>& infos);

bool parseDigestValue(const QString& word, QString& name, QString& value);


void populateDigestTable(const QList<SIPCommaValue>& values,
                         std::map<QString, QString> &table, bool expectDigest);

QString getDigestTableValue(const std::map<QString, QString> &table, const QString& name);


bool parseDigestChallengeField(const SIPField &field,
                               QList<DigestChallenge> &dChallenge);

bool parseDigestResponseField(const SIPField &field,
                              QList<DigestResponse> &dResponse);

