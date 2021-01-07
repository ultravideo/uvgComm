#pragma once

#include "initiation/siptypes.h"


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
