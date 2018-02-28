#pragma once

#include <siptypes.h>

// request and string
RequestType stringToRequest(QString request);
QString requestToString(RequestType request);

// Response code and string
uint16_t stringToResponseCode(QString code);

// Response and response code
ResponseType codeToResponse(uint16_t code);
uint16_t responseToCode(ResponseType response);

// phrase and response code
QString codeToPhrase(uint16_t code);
QString responseToPhrase(ResponseType response);

// connection type and string
ConnectionType stringToConnection(QString type);
QString connectionToString(ConnectionType connection);
