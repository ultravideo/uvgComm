#pragma once

#include "initiation/siptypes.h"

// Returns if request makes sense over all. These functions also remove all
// invalid fields from the message.
bool requestSanityCheck(QList<SIPField>& fields, SIPRequestMethod method);
bool responseSanityCheck(QList<SIPField>& fields, SIPResponseStatus status);
