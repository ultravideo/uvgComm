#pragma once

#include "siptypes.h"



bool includeToField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeFromField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeCSeqField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeCallIDField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeViaField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeMaxForwardsField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeContactField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeContentTypeField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);

bool includeContentLengthField(QList<SIPField>& fields,
                  std::shared_ptr<SIPMessageInfo> message);
