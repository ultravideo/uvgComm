#pragma once

#include "initiation/siptypes.h"

/* A module for parsing SIP header fields SIP message, but not the first line.
 *
 * returns whether the parsing was successful.
 *
 * Currently these parse the whole comma separated list, but it would probably
 *  be better if they parsed only one value from the list at a time. No need
 *   to loop through the list in every one of these
 *
 *
 * Please call check parsing possibility with parsingPreCheck before calling any
 * of the parsing functions.
 *
*/


// checks parsing preconditions such as whether message exists and whether all
// word lists have words in them
bool parsingPreChecks(const SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message,
                      bool emptyPossible = false);

bool parseAcceptField(const SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message);

bool parseAcceptEncodingField(const SIPField& field,
                              std::shared_ptr<SIPMessageHeader> message);

bool parseAcceptLanguageField(const SIPField& field,
                              std::shared_ptr<SIPMessageHeader> message);

bool parseAlertInfoField(const SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

bool parseAllowField(const SIPField& field,
                     std::shared_ptr<SIPMessageHeader> message);

bool parseAuthInfoField(const SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message);

bool parseAuthorizationField(const SIPField& field,
                             std::shared_ptr<SIPMessageHeader> message);

bool parseCallIDField(const SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message);

bool parseCallInfoField(const SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message);

bool parseContactField(const SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message);

bool parseContentDispositionField(const SIPField& field,
                                  std::shared_ptr<SIPMessageHeader> message);

bool parseContentEncodingField(const SIPField& field,
                               std::shared_ptr<SIPMessageHeader> message);

bool parseContentLanguageField(const SIPField& field,
                               std::shared_ptr<SIPMessageHeader> message);

bool parseContentLengthField(const SIPField& field,
                             std::shared_ptr<SIPMessageHeader> message);

bool parseContentTypeField(const SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseCSeqField(const SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message);

bool parseDateField(const SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message);

bool parseErrorInfoField(const SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

bool parseExpireField(const SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message);

bool parseFromField(const SIPField& field,
                    std::shared_ptr<SIPMessageHeader> message);

bool parseInReplyToField(const SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

bool parseMaxForwardsField(const SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseMinExpiresField(const SIPField& field,
                          std::shared_ptr<SIPMessageHeader> message);

bool parseMIMEVersionField(const SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseOrganizationField(const SIPField& field,
                            std::shared_ptr<SIPMessageHeader> message);

bool parsePriorityField(const SIPField& field,
                        std::shared_ptr<SIPMessageHeader> message);

bool parseProxyAuthenticateField(const SIPField& field,
                                 std::shared_ptr<SIPMessageHeader> message);

bool parseProxyAuthorizationField(const SIPField& field,
                                  std::shared_ptr<SIPMessageHeader> message);

bool parseProxyRequireField(const SIPField& field,
                            std::shared_ptr<SIPMessageHeader> message);

bool parseRecordRouteField(const SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseReplyToField(const SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message);

bool parseRequireField(const SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message);

bool parseRetryAfterField(const SIPField& field,
                          std::shared_ptr<SIPMessageHeader> message);

bool parseRouteField(const SIPField& field,
                     std::shared_ptr<SIPMessageHeader> message);

bool parseServerField(const SIPField& field,
                      std::shared_ptr<SIPMessageHeader> message);

bool parseSubjectField(const SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message);

bool parseSupportedField(const SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

bool parseTimestampField(const SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

bool parseToField(const SIPField& field,
                  std::shared_ptr<SIPMessageHeader> message);

bool parseUnsupportedField(const SIPField& field,
                           std::shared_ptr<SIPMessageHeader> message);

bool parseUserAgentField(const SIPField& field,
                         std::shared_ptr<SIPMessageHeader> message);

bool parseViaField(const SIPField& field,
                   std::shared_ptr<SIPMessageHeader> message);

bool parseWarningField(const SIPField& field,
                       std::shared_ptr<SIPMessageHeader> message);

bool parseWWWAuthenticateField(const SIPField& field,
                               std::shared_ptr<SIPMessageHeader> message);
