#pragma once

#include "initiation/siptypes.h"


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding);
bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding);

// RFC3261_TODO: Accept header would be nice

// These functions work as follows: Create a field based on necessary info
// from the message and add the field to list. Later the fields are converted to string.

bool includeToField(QList<SIPField>& fields,
                    std::shared_ptr<SIPMessageBody> message);

bool includeFromField(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageBody> message);

bool includeCSeqField(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageBody> message);

bool includeCallIDField(QList<SIPField>& fields,
                        std::shared_ptr<SIPMessageBody> message);

bool includeViaFields(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageBody> message);

bool includeMaxForwardsField(QList<SIPField>& fields,
                             std::shared_ptr<SIPMessageBody> message);

bool includeContactField(QList<SIPField>& fields,
                         std::shared_ptr<SIPMessageBody> message);

bool includeContentTypeField(QList<SIPField>& fields,
                             QString contentType);

bool includeContentLengthField(QList<SIPField>& fields,
                               uint32_t contentLenght);

bool includeExpiresField(QList<SIPField>& fields,
                         uint32_t expires);

bool includeRecordRouteField(QList<SIPField>& fields,
                             std::shared_ptr<SIPMessageBody> message);

bool includeRouteField(QList<SIPField>& fields,
                       std::shared_ptr<SIPMessageBody> message);
