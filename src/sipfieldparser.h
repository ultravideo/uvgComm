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

void parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseFromField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseCallIDField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseViaField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseMaxForwardsField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseContactField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseContentTypeField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);

void parseContentLengthField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message);


bool isLinePresent(QString name, QList<SIPField> &fields);

bool parseParameter(QString text, SIPParameter& parameter);
