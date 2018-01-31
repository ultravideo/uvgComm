#pragma once

#include <siptypes.h>

RequestType stringToRequest(QString request);
QString requestToString(RequestType request);

QString responseCodeToString(uint16_t code);
uint16_t stringResponseCode(QString code);

ResponseType codeToResponse(uint16_t code);
uint16_t responseToCode(ResponseType response);

QString codeToPhrase(uint16_t code);
QString responseToPhrase(ResponseType response);
