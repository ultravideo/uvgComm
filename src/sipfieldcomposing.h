#pragma once

#include "siptypes.h"


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding);
bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding);

bool includeToField(QList<SIPField>& fields,
                    std::shared_ptr<SIPMessageInfo> message);

bool includeFromField(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageInfo> message);

bool includeCSeqField(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageInfo> message);

bool includeCallIDField(QList<SIPField>& fields,
                        std::shared_ptr<SIPMessageInfo> message);

bool includeViaFields(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageInfo> message);

bool includeMaxForwardsField(QList<SIPField>& fields,
                             std::shared_ptr<SIPMessageInfo> message);

bool includeContactField(QList<SIPField>& fields,
                         std::shared_ptr<SIPMessageInfo> message);

bool includeContentTypeField(QList<SIPField>& fields,
                             QString contentType);

bool includeContentLengthField(QList<SIPField>& fields,
                               uint32_t contentLenght);
