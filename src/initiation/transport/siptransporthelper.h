#pragma once

#include "initiation/siptypes.h"

#include <QVariant>

// ===== Composing =====

// compose fields that are always included in a message
bool composeMandatoryFields(QList<SIPField>& fields,
                            std::shared_ptr<SIPMessageHeader> header);

// convert fields table to one string which is returned
QString fieldsToString(QList<SIPField>& fields, QString lineEnding);

// returns the content as string and adds content-type/length to fields
QString addContent(QList<SIPField>& fields, const std::shared_ptr<SIPMessageHeader> header,
                   QVariant &content);


// ===== Parsing =====

// parses the string to fields. Combines fields spanning multiple lines and divides
// the fields to valuesets separated by comma(,)
bool headerToFields(QString &header, QString& firstLine, QList<SIPField> &fields);

// converts the fields to SIPMessageHeader struct
bool fieldsToMessageHeader(QList<SIPField>& fields,
                           std::shared_ptr<SIPMessageHeader> &header);

// parses the body to content based on mediatype
void parseContent(QVariant &content, MediaType mediaType, QString &body);
