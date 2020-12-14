#pragma once

/* A module for parsing various parts of SIP message. When adding support for a new field,
 * add function here and add a pointer to the map in siptransport.cpp. */

#include "initiation/siptypes.h"

// parsing of individual header fields to SDPMessage, but not the first line.
// returns whether the parsing was successful.

bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageBody> message);

bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageBody> message);

bool parseCSeqField(SIPField& field,
                    std::shared_ptr<SIPMessageBody> message);

bool parseCallIDField(SIPField& field,
                    std::shared_ptr<SIPMessageBody> message);

bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageBody> message);

bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageBody> message);

bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageBody> message);

bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageBody> message);

bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageBody> message);

bool parseRecordRouteField(SIPField& field,
                           std::shared_ptr<SIPMessageBody> message);

bool parseServerField(SIPField& field,
                      std::shared_ptr<SIPMessageBody> message);

bool parseUserAgentField(SIPField& field,
                         std::shared_ptr<SIPMessageBody> message);

bool parseUnimplemented(SIPField& field,
                        std::shared_ptr<SIPMessageBody> message);

// check if the SIP message contains all required fields
bool checkRequestMustFields(QList<SIPField>& fields, SIPRequestMethod method);
bool checkResponseMustFields(QList<SIPField>& fields, SIPResponseStatus status,
                             SIPRequestMethod ongoingTransaction);

// check if the field makes sense in this request/response.
// Fields that do not should be ignored.
bool sensibleRequestField(QList<SIPField>& fields, SIPRequestMethod method, QString field);
bool sensibleResponseField(QList<SIPField>& fields, SIPResponseStatus status,
                           SIPRequestMethod ongoingTransaction, QString field);


// tell whether a particular field is present in list.
bool isLinePresent(QString name, QList<SIPField> &fields);

int countVias(QList<SIPField> &fields);

// takes the parameter string (name=value) and parses it to SIPParameter
// used by parse functions.
bool parseParameter(QString text, SIPParameter& parameter);
