#pragma once

#include "initiation/siptypes.h"

// various helper functions associated with SIP

// request and string
SIPRequestMethod stringToRequestMethod(const QString& request);
QString requestMethodToString(SIPRequestMethod request);

// Response code and string
uint16_t stringToResponseCode(const QString& code);

// Response type, response code and phrase conversions
SIPResponseStatus codeToResponseType(uint16_t code);
uint16_t responseTypeToCode(SIPResponseStatus response);
QString codeToPhrase(uint16_t code);
QString responseTypeToPhrase(SIPResponseStatus response);

// connection type and string
SIPTransportProtocol stringToTransportProtocol(const QString& type);
QString transportProtocolToString(const SIPTransportProtocol connection);

// contentType
MediaType stringToContentType(const QString& typeStr);
QString contentTypeToString(const MediaType type);

// Message QOP
QopValue stringToQopValue(const QString& qop);
QString qopValueToString(const QopValue qop);

// Digest Algorithm
DigestAlgorithm stringToAlgorithm(const QString& algorithm);
QString algorithmToString(const DigestAlgorithm algorithm);

bool stringToBool(const QString& boolean, bool& ok);
QString boolToString(const bool boolean);


SIPPriorityField stringToPriority(const QString& priority);
QString priorityToString(const SIPPriorityField priority);





// this is here because it is used by both composing and parsing
bool addParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                  const SIPParameter &parameter);

// this is here because it is used by both composing and parsing
void copyParameterList(const std::shared_ptr<QList<SIPParameter> > &inParameters,
                       std::shared_ptr<QList<SIPParameter> > &outParameters);
