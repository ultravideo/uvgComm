#pragma once

/* A module for parsing various parts of SIP message. When adding support for a new field,
 * add function here and add a pointer to the map in siptransport.cpp. */

#include "initiation/siptypes.h"

// parsing of individual header fields to SDPMessage, but not the first line.
// returns whether the parsing was successful.

bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message);

bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message);

bool parseCSeqField(SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message);

bool parseCallIDField(SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message);

bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageHeader> message);

bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message);

bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageHeader> message);

bool parseRecordRouteField(SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseServerField(SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message);

bool parseUserAgentField(SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

// this is a placeholder function. Serves no actual purpose
bool parseUnimplemented(SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message);

