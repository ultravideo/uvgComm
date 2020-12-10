#pragma once

#include "initiation/siptypes.h"

// various helper functions associated with SIP

// request and string
SIPRequestMethod stringToRequest(QString request);
QString requestToString(SIPRequestMethod request);

// Response code and string
uint16_t stringToResponseCode(QString code);

// Response and response code
SIPResponseStatus codeToResponse(uint16_t code);
uint16_t responseToCode(SIPResponseStatus response);

// phrase and response code
QString codeToPhrase(uint16_t code);
QString responseToPhrase(SIPResponseStatus response);

// connection type and string
Transport stringToConnection(QString type);
QString connectionToString(Transport connection);

// contentType
ContentType stringToContentType(QString typeStr);
QString contentTypeToString(ContentType type);
