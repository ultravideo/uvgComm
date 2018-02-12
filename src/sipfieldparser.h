#pragma once

/* A module for parsing various parts of SIP message. When adding support for a new field, add function here
and add a pointer to the map in sipconnection.cpp. */

#include <siptypes.h>

struct SIPParameter
{
  QString name;
  QString value;
};

struct SIPField
{
  QString name;
  QString values;
  std::shared_ptr<QList<SIPParameter>> parameters;
};

// parsing of individual header fields, but not the first line.
// returns whther the parsing was successful.

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


bool isLinePresent(QString name, QList<SIPField> &fields);

// takes the parameter string (name=value) and parses it to SIPParameter
bool parseParameter(QString text, SIPParameter& parameter);
