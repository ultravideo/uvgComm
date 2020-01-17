#pragma once

/* A module for parsing various parts of SIP message. When adding support for a new field,
 * add function here and add a pointer to the map in siptransport.cpp. */

#include "initiation/siptypes.h"

// parsing of individual header fields to SDPMessage, but not the first line.
// returns whether the parsing was successful.

bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseFromField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseCallIDField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseViaField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseMaxForwardsField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseContactField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseContentTypeField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseContentLengthField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseServerField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

bool parseUserAgentField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);



// tell whether a particular field is present in list.
bool isLinePresent(QString name, QList<SIPField> &fields);

// takes the parameter string (name=value) and parses it to SIPParameter
// used by parse functions.
bool parseParameter(QString text, SIPParameter& parameter);
