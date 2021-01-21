#pragma once

#include "initiation/siptypes.h"

#include <map>

bool parseURI(const QString& word, SIP_URI& uri);
bool parseAbsoluteURI(const QString& word, AbsoluteURI& uri);
bool parseNameAddr(const QStringList &words, NameAddr& nameAddr);
bool parseSIPRouteLocation(const SIPCommaValue &value, SIPRouteLocation& location);
bool parseSIPRouteList(const SIPField& field, QList<SIPRouteLocation>& list);

bool parseUritype(const QString& type, SIPType &out_Type);
bool parseParameterByName(const QList<SIPParameter> &parameters,
                          QString name, QString& value);

bool parseFloat(const QString& string, float& value);
bool parseUint8(const QString& values, uint8_t& number);
bool parseUint16(const QString& values, uint16_t& number);
bool parseUint32(const QString& values, uint32_t& number);
bool parseUint64(const QString& values, uint64_t& number);


bool parseSharedUint32(const SIPField& field, std::shared_ptr<uint32_t>& value);


// takes the parameter string (name=value) and parses it to SIPParameter
// used by parse functions.
bool parseParameter(const QString& text, SIPParameter& parameter);


bool parseAcceptGeneric(const SIPField& field,
                        std::shared_ptr<QList<SIPAcceptGeneric>> generics);

bool parseInfo(const SIPField& field,
               QList<SIPInfo>& infos);

bool parseDigestValue(const QString& word, QString& name, QString& value);


void populateDigestTable(const QList<SIPCommaValue>& values,
                         std::map<QString, QString> &table, bool expectDigest);

QString getDigestTableValue(const std::map<QString, QString> &table, const QString& name);


bool parseDigestChallengeField(const SIPField &field,
                               QList<DigestChallenge> &dChallenge);

bool parseDigestResponseField(const SIPField &field,
                              QList<DigestResponse> &dResponse);


bool parseStringList(const SIPField& field, QStringList& list);


bool parseString(const SIPField& field, QString &value, bool allowEmpty);
