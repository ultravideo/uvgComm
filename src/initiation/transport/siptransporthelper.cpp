#include "siptransporthelper.h"

#include "sipfieldparsing.h"
#include "sipfieldcomposing.h"

#include "initiation/negotiation/sdptypes.h"
#include "initiation/negotiation/sipcontent.h"

#include "common.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>


// one letter forms are not currently included in composing

// TODO: Set order so that those fields needed by proxy are close to the top

// NOTE: This will be the order in which fields will appear in SIP message
const std::vector<std::pair<QString,
                            std::function<bool(QList<SIPField>& fields,
                                               std::shared_ptr<SIPMessageHeader>)>>> composing =
{
    {"Accept",              includeAcceptField},
    {"Accept-Encoding",     includeAcceptEncodingField},
    {"Accept-Language",     includeAcceptLanguageField},
    {"Alert-Info",          includeAlertInfoField},
    {"Allow",               includeAllowField},
    {"Authentication-Info", includeAuthInfoField},
    {"Authorization",       includeAuthorizationField},
    {"Call-ID",             includeCallIDField},
    {"Call-Info",           includeCallInfoField},
    {"Contact",             includeContactField},
    {"Content-Disposition", includeContentDispositionField},
    {"Content-Encoding",    includeContentEncodingField},
    {"Content-Language",    includeContentLanguageField},
    {"Content-Length",      includeContentLengthField},
    {"Content-Type",        includeContentTypeField},
    {"CSeq",                includeCSeqField},
    {"Date",                includeDateField},
    {"Error-Info",          includeErrorInfoField},
    {"Expires",             includeExpiresField},
    {"From",                includeFromField},
    {"In-Reply_to",         includeInReplyToField},
    {"Max-Forwards",        includeMaxForwardsField},
    {"Min-Expires",         includeMinExpiresField},
    {"MIME-Version",        includeMIMEVersionField},
    {"Organization",        includeOrganizationField},
    {"Priority",            includePriorityField},
    {"Proxy-Authenticate",  includeProxyAuthenticateField},
    {"Proxy-Authorization", includeProxyAuthorizationField},
    {"Proxy-Require",       includeProxyRequireField},
    {"Record-Route",        includeRecordRouteField},
    {"Reply-To",            includeReplyToField},
    {"Require",             includeRequireField},
    {"Retry-After",         includeRetryAfterField},
    {"Route",               includeRouteField},
    {"Server",              includeServerField},
    {"Subject",             includeSubjectField},
    {"Supported",           includeSupportedField},
    {"Timestamp",           includeTimestampField},
    {"To",                  includeToField},
    {"Unsupported",         includeUnsupportedField},
    {"User-Agent",          includeUserAgentField},
    {"Via",                 includeViaFields},
    {"Warning",             includeWarningField},
    {"WWW-Authenticate",    includeWWWAuthenticateField}
};


// one letter headers are compact forms as defined by RFC 3261

// TODO: The case should be uniformalized and these should all be small or big case
const std::map<QString, std::function<bool(SIPField& field,
                                           std::shared_ptr<SIPMessageHeader>)>> parsing =
{
    {"Accept",              parseAcceptField},
    {"Accept-Encoding",     parseAcceptEncodingField},
    {"Accept-Language",     parseAcceptLanguageField},
    {"Alert-Info",          parseAlertInfoField},
    {"Allow",               parseAllowField},
    {"Authentication-Info", parseAuthInfoField},
    {"Authorization",       parseAuthorizationField},
    {"Call-ID",             parseCallIDField},
    {"i",                   parseCallIDField},          // compact form of Call-ID
    {"Call-Info",           parseCallInfoField},
    {"Contact",             parseContactField},
    {"m",                   parseContactField},         // compact form of contact
    {"Content-Disposition", parseContentDispositionField},
    {"Content-Encoding",    parseContentEncodingField},
    {"e",                   parseContentEncodingField}, // compact form of Content-Encoding
    {"Content-Language",    parseContentLanguageField},
    {"Content-Length",      parseContentLengthField},
    {"l",                   parseContentLengthField},   // compact form of Content-Length
    {"Content-Type",        parseContentTypeField},
    {"c",                   parseContentTypeField},     // compact form of Content-Type
    {"CSeq",                parseCSeqField},
    {"Date",                parseDateField},
    {"Error-Info",          parseErrorInfoField},
    {"Expires",             parseExpireField},
    {"From",                parseFromField},
    {"f",                   parseFromField},            // compact form of From
    {"In-Reply_to",         parseInReplyToField},
    {"Max-Forwards",        parseMaxForwardsField},
    {"Min-Expires",         parseMinExpiresField},
    {"MIME-Version",        parseMIMEVersionField},
    {"Organization",        parseOrganizationField},
    {"Priority",            parsePriorityField},
    {"Proxy-Authenticate",  parseProxyAuthenticateField},
    {"Proxy-Authorization", parseProxyAuthorizationField},
    {"Proxy-Require",       parseProxyRequireField},
    {"Record-Route",        parseRecordRouteField},
    {"Reply-To",            parseReplyToField},
    {"Require",             parseRequireField},
    {"Retry-After",         parseRetryAfterField},
    {"Route",               parseRouteField},
    {"Server",              parseServerField},
    {"Subject",             parseSubjectField},
    {"s",                   parseSubjectField},        // compact form of Subject
    {"Supported",           parseSupportedField},
    {"k",                   parseSupportedField},      // compact form of Supported
    {"Timestamp",           parseTimestampField},
    {"To",                  parseToField},
    {"t",                   parseToField},             // compact form of To
    {"Unsupported",         parseUnsupportedField},
    {"User-Agent",          parseUserAgentField},
    {"Via",                 parseViaField},
    {"v",                   parseViaField},            // compact form of Via
    {"Warning",             parseWarningField},
    {"WWW-Authenticate",    parseWWWAuthenticateField}
};

bool combineContinuationLines(QStringList& lines);

bool parseFieldName(QString& line, SIPField &field);
void parseFieldCommaSeparatedList(QString& line, QStringList &outValues);
bool parseFieldValue(QString& values, SIPField& field);

void addParameterToSet(SIPParameter& currentParameter, QString& currentWord,
                       SIPCommaValue& value);


void composeAllFields(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageHeader> header)
{
  for (auto& field : composing)
  {
    if (field.second(fields, header))
    {
      printNormal("SIP Transport Helper", "Composed a field", "Field", field.first);
    }
  }
}


QString fieldsToString(QList<SIPField>& fields, QString lineEnding)
{
  QString message = "";
  for(SIPField& field : fields)
  {
    if (field.name != "")
    {
      message += field.name + ": ";

      for (int i = 0; i < field.commaSeparated.size(); ++i)
      {
        // add words.
        for (int j = 0; j < field.commaSeparated.at(i).words.size(); ++j)
        {
          message += field.commaSeparated.at(i).words.at(j);

          if (j != field.commaSeparated.at(i).words.size() - 1)
          {
            message += " ";
          }
        }

        // add parameters
        if(field.commaSeparated.at(i).parameters != nullptr)
        {
          for(SIPParameter& parameter : *field.commaSeparated.at(i).parameters)
          {
            if (parameter.value != "")
            {
              message += ";" + parameter.name + "=" + parameter.value;
            }
            else
            {
              message += ";" + parameter.name;
            }
          }
        }

        // add comma(,) if not the last value
        if (i != field.commaSeparated.size() - 1)
        {
          message += ",";
        }
      }

      message += lineEnding;
    }
    else
    {
      printProgramError("SIP Transport Helper", "Failed to convert field to string",
                        "Field name", field.name);
    }
  }
  return message;
}


QString addContent(const std::shared_ptr<SIPMessageHeader> header,
                   QVariant &content)
{
  // TODO: QString is probably not the right container for content
  // since it can also be audio or video. QByteArray would probably be better

  QString contentString = "";

  if (header->contentType == MT_TEXT)
  {
    contentString = content.value<QString>();
  }
  else if(header->contentType == MT_APPLICATION_SDP)
  {
    contentString = composeSDPContent(content.value<SDPMessageInfo>());
  }

  header->contentLength = contentString.length();

  return contentString;
}


bool headerToFields(QString& header, QString& firstLine, QList<SIPField>& fields)
{
  // Divide into lines
  QStringList lines = header.split("\r\n", QString::SkipEmptyParts);
  printNormal("SIP Transport Helper", "Parsing SIP header to fields",
              "Fields", QString::number(lines.size()));
  if(lines.size() == 0)
  {
    printPeerError("SIP Transport Helper", "No first line present in SIP header!");
    return false;
  }

  // TODO: Combine multiple fields into one comma separated list.
  // Expect for WWW-Authenticate, Authorization,
  // Proxy-Authenticate, and Proxy-Authorization

  // The message may contain fields that extend more than one line.
  // Combine them so field is only present on one line
  combineContinuationLines(lines);
  firstLine = lines.at(0);

  QStringList debugLineNames = {};
  for(int i = 1; i < lines.size(); ++i)
  {
    SIPField field = {"", {}};
    QStringList commaSeparated;

    if (parseFieldName(lines[i], field))
    {
      parseFieldCommaSeparatedList(lines[i], commaSeparated);

      // Check the correct number of values for Field
      if (commaSeparated.size() > 500)
      {
        printDebug(DEBUG_PEER_ERROR, "SIP Transport Helper",
                   "Too many comma separated sets in field",
                    {"Field", "Amount"}, {field.name, QString::number(commaSeparated.size())});
        return false;
      }

      for (QString& value : commaSeparated)
      {
        if (!parseFieldValue(value, field))
        {
          printWarning("SIP Transport Helper", "Failed to parse field",
                       "Name", field.name);
          return false;
        }
      }

      debugLineNames << field.name;
      fields.push_back(field);
    }
    else
    {
      return false;
    }
  }

  printDebug(DEBUG_NORMAL, "SIP Transport Helper", "Found the following lines",
             {"Lines"}, debugLineNames);

  return true;
}


bool fieldsToMessageHeader(QList<SIPField>& fields,
                           std::shared_ptr<SIPMessageHeader>& header)
{
  header->cSeq.cSeq = 0;
  header->cSeq.method = SIP_NO_REQUEST;
  header->maxForwards = 0;
  header->to =   {{"",SIP_URI{}}, ""};
  header->from = {{"",SIP_URI{}}, ""};
  header->contentType = MT_NONE;
  header->contentLength = 0;
  header->expires = 0;

  for(int i = 0; i < fields.size(); ++i)
  {
    if(parsing.find(fields[i].name) == parsing.end())
    {
      printWarning("SIP Transport Helper", "Field not supported",
                   "Name", fields[i].name);
    }
    else if(!parsing.at(fields.at(i).name)(fields[i], header))
    {
      printWarning("SIP Transport Helper", "Failed to parse following field",
                   "Name", fields[i].name);
      return false;
    }
  }
  return true;
}


void parseContent(QVariant& content, MediaType mediaType, QString& body)
{
  if(mediaType == MT_APPLICATION_SDP)
  {
    SDPMessageInfo sdp;
    if(parseSDPContent(body, sdp))
    {
      printNormal("SIP Transport Helper", "Successfully parsed SDP");
      content.setValue(sdp);
    }
    else
    {
      printWarning("SIP Transport Helper", "Failed to parse SDP message");
    }
  }
  else
  {
    printWarning("SIP Transport Helper", "Unsupported content type detected!");
  }
}


bool combineContinuationLines(QStringList& lines)
{
  for (int i = 1; i < lines.size(); ++i)
  {
    // combine current line with previous if there a space at the beginning
    if (lines.at(i).front().isSpace())
    {
      printNormal("SIP Transport Helper", "Found a continuation line");
      lines[i - 1].append(lines.at(i));
      lines.erase(lines.begin() + i);
      --i;
    }
  }

  return true;
}


bool parseFieldName(QString& line, SIPField& field)
{
  QRegularExpression re_field("(\\S*):\\s+(.*)");
  QRegularExpressionMatch field_match = re_field.match(line);

  // separate name from rest
  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
  {
    field.name = field_match.captured(1);
    line = field_match.captured(2);
  }
  else
  {
    return false;
  }
  return true;
}


void parseFieldCommaSeparatedList(QString& line, QStringList& outValues)
{
  // separate value sections by commas
  outValues = line.split(",", QString::SkipEmptyParts);
}


bool parseFieldValue(QString& values, SIPField& field)
{
  // RFC3261_TODO: Uniformalize case formatting. Make everything big or small case expect quotes.
  SIPCommaValue set = SIPCommaValue{{}, nullptr};

  QString currentWord = "";
  bool isQuotation = false;
  bool isURI = false;
  bool isParameter = false;
  int comments = 0;

  SIPParameter parameter;

  for (QChar& character : values)
  {
    // add character to word if it is not parsed out
    if (isURI || (isQuotation && character != "\"") ||
        (character != " "
        && character != "\""
        && character != ";"
        && character != "="
        && character != "("
        && character != ")"
        && !comments))
    {
      currentWord += character;
    }

    // push current word if it ended
    if (!comments)
    {
      if ((character == "\"" && isQuotation) ||
          (character == ">" && isURI))
      {
        if (!isParameter && currentWord != "")
        {
          set.words.push_back(currentWord);
          currentWord = "";
        }
        else
        {
          return false;
        }
      }
      else if (character == " " && !isQuotation)
      {
        if (!isParameter && currentWord != "")
        {
          set.words.push_back(currentWord);
        }
        currentWord = "";
      }
      else if((character == "=" && isParameter) ||
         (character == ";" && !isURI))
      {
        if (!isParameter)
        {
          if (character == ";" && currentWord != "")
          {
            // last word before parameters
            set.words.push_back(currentWord);
            currentWord = "";
          }
        }
        else // isParameter
        {
          if (character == "=")
          {
            if (parameter.name != "")
            {
              return false; // got name when we already have one
            }
            else
            {
              parameter.name = currentWord;
              currentWord = "";
            }
          }
          else if (character == ";")
          {
            addParameterToSet(parameter, currentWord, set);
          }
        }
      }
    }

    // change the state of parsing
    if (character == "\"") // quote
    {
      isQuotation = !isQuotation;
    }
    else if (character == "<") // start of URI
    {
      if (isQuotation || isURI)
      {
        return false;
      }
      isURI = true;
    }
    else if (character == ">") // end of URI
    {
      if (!isURI || isQuotation)
      {
        return false;
      }
      isURI = false;
    }
    else if (character == "(")
    {
      ++comments;
    }
    else if (character == ")")
    {
      if (!comments)
      {
        return false;
      }

      --comments;
    }
    else if (!isURI && !isQuotation && character == ";") // parameter
    {
      isParameter = true;
    }
  }

  // add last word
  if (isParameter)
  {
    addParameterToSet(parameter, currentWord, set);
  }
  else if (currentWord != "")
  {
    set.words.push_back(currentWord);
    currentWord = "";
  }

  field.commaSeparated.push_back(set);

  return true;
}


void addParameterToSet(SIPParameter& currentParameter, QString &currentWord,
                       SIPCommaValue& value)
{
  if (currentParameter.name == "")
  {
    currentParameter.name = currentWord;
  }
  else
  {
    currentParameter.value = currentWord;
  }
  currentWord = "";

  if (value.parameters == nullptr)
  {
    value.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  value.parameters->push_back(currentParameter);
  currentParameter = SIPParameter{"",""};
}

