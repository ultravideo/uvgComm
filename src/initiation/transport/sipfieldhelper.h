#pragma once

#include "initiation/siptypes.h"


// ========= Composing functions =========

// a function used within this file to add a parameter
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName, QString parameterValue);
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName);

QString composeUritype(SIPType type);
bool composeSIPUri(const SIP_URI &uri, QStringList& words);
bool composeNameAddr(const NameAddr &nameAddr, QStringList& words);
QString composePortString(uint16_t port);

bool composeSIPRouteLocation(const SIPRouteLocation &location,
                             SIPValueSet& valueSet);


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
                      std::shared_ptr<SIPMessageHeader> message);

// takes the parameter string (name=value) and parses it to SIPParameter
// used by parse functions.
bool parseParameter(QString text, SIPParameter& parameter);
