#pragma once

#include "initiation/siptypes.h"

// Returns if request makes sense over all. These functions also remove all
// invalid fields from the message.
bool requestSanityCheck(QList<SIPField>& fields, SIPRequestMethod method);
bool responseSanityCheck(QList<SIPField>& fields, SIPResponseStatus status);


// check if the field makes sense in this request/response.
// Fields that do not should be ignored.
bool sensibleRequestField(SIPRequestMethod method,
                          QString field);

bool sensibleResponseField(SIPResponseStatus status,
                           SIPRequestMethod ongoingTransaction,
                           QString field);
