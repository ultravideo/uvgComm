#include "sipparser.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QList>
#include <QHostInfo>

#include <iostream>
#include <sstream>
#include <string>

struct HeaderLine
{
  QString name;
  quint8 words;
};

//TODO: possiblity of having more than one same line in header.

// remember to also update tableToInfo if adding new header fields!
const QList<HeaderLine> SIPHEADERLINES(
{
      {"METHOD", 3},
      {"Via:", 3},
      {"Max-Forwards:", 2},
      {"To:", 3},
      {"From:", 3},
      {"Call-ID:", 2},
      {"Cseq:", 3},
      {"Contact:", 2},
      {"Content-Type:", 2},
      {"Content-Length:", 2},
}
);

// TODO this changes depending on the message type, maybe make a function/table for that
const uint16_t SIPMANDATORYLINES = 9;


// Functions


void messageToTable(QStringList& lines, QList<QStringList> &values);
SIPMessageInfo* tableToInfo(QList<QStringList>& values);
bool checkSIPMessage(QList<QStringList>& values);

void parseSIPaddress(QString address, QString& user, QString& location);
bool parseSIPParameter(QString field, QString parameterName,
                       QString& parameterValue, QString& remaining);

void cleanup(SIPMessageInfo* info);


bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue);

bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue)
{
  Q_ASSERT(expectedLength != 0);

  if(line.size() != expectedLength)
  {
    qDebug() << "SDP line:" << line << "not expected length:" << expectedLength;
    return false;
  }
  if(line.at(0).length() < 3)
  {
    qDebug() << "First word missing in SDP Line:" << line;
    return false;
  }

  firstValue = line.at(0).right(line.at(0).size() - 2);

  return true;
}

void cleanup(SIPMessageInfo* info)
{
  if(info != 0)
    delete info;
}

std::shared_ptr<SIPMessageInfo> parseSIPMessage(QString& header)
{
  QStringList lines = header.split("\r\n");

  qDebug() << "SIP message split into"  << lines.length() << "lines";

  QList<QStringList> values;

  // increase the list size
  while(values.size() < SIPHEADERLINES.size())
  {
    values.append(QStringList(""));
  }

  messageToTable(lines, values);
  if(!checkSIPMessage(values))
    return NULL;

  SIPMessageInfo* info = tableToInfo(values);

  return std::shared_ptr<SIPMessageInfo> (info);
}

void messageToTable(QStringList& lines, QList<QStringList> &values)
{
  qDebug() << "Sorting sip message to table for lookup";

  for(uint32_t i = 0;  i < lines.length(); ++i)
  {
    qDebug() << "Line" << i << "contents:" << lines.at(i);
    QStringList words = lines.at(i).split(" ");

    if(i == 0)
    {
      values[0] = words;
    }
    else
    {
      // find headertype in array
      for(uint32_t j = 1; j < SIPHEADERLINES.size(); ++j)
      {
        // RFC-3261 defines headers as case-insensitive
        if(QString::compare(words.at(0), SIPHEADERLINES.at(j).name, Qt::CaseInsensitive) == 0)
        {
          values[j] = words;
        }
      }
    }
  }
}

SIPMessageInfo* tableToInfo(QList<QStringList>& values)
{
  SIPMessageInfo* info = new SIPMessageInfo;

  qDebug() << "Converting table to struct";

  if(values.at(0).at(0) == "INVITE")
  {
    info->request = INVITE;
    qDebug() << "INVITE found";

    // TODO: this affects which header-lines are mandatory,
    //       and it should be taken into account
  }
  else
  {
    qDebug() << "Unrecognized Method:" << values.at(0).at(0);
    cleanup(info);
    return NULL;
  }

  // values has been set according to HEADERLINES-table
  // and their existance has been checked in checkSIPMessage-function
  info->version = values.at(0).at(2).right(3);
  qDebug() << "Version:" << info->version;

  info->theirName = values.at(4).at(1);
  info->ourName = values.at(3).at(1);

  QString replyAddress = "";
  parseSIPParameter(values.at(1).at(2), ";branch=", info->branch, replyAddress);

  if(STRICTSIPPROCESSING && info->branch.isEmpty())
  {
    cleanup(info);
    return NULL;
  }

  //info->replyAddress = parseIPAddress(replyAddress);
  info->replyAddress = replyAddress;

  QStringList callIDsplit = values.at(5).at(1).split("@");

  if(callIDsplit.size() != 2)
  {
    qDebug() << "Unclear CallID:" << callIDsplit;
    if(STRICTSIPPROCESSING || callIDsplit.size() > 2)
    {
      cleanup(info);
      return NULL;
    }
    info->host = "";
  }
  else
  {
    info->host = callIDsplit.at(1);
  }

  info->callID = callIDsplit.at(0);

  qDebug() << "CallID:" << info->callID;

  QString toSIPAddress = "";
  QString fromSIPAddress = "";

  parseSIPParameter(values.at(3).at(2), ";tag=", info->ourTag, toSIPAddress);
  parseSIPParameter(values.at(4).at(2), ";tag=", info->theirTag, fromSIPAddress);

  if(STRICTSIPPROCESSING && info->theirTag.isEmpty())
  {
    qDebug() << "They did not send their tag!";
    cleanup(info);
    return NULL;
  }

  parseSIPaddress(toSIPAddress, info->ourUsername, info->ourLocation);
  parseSIPaddress(fromSIPAddress, info->theirUsername, info->theirLocation);

  bool ok = false;
  info->cSeq = values.at(6).at(1).toInt(&ok);

  if(!ok && STRICTSIPPROCESSING)
  {
    qDebug() << "Some junk in cSeq:" << info->cSeq;
    delete info;
    return NULL;
  }

  QString username ="";
  QString contactAddress = "";

  parseSIPaddress(values.at(7).at(1), username, contactAddress);

  //info->contactAddress = parseIPAddress(contactAddress);
  info->contactAddress = contactAddress;


  info->contentType = values.at(8).at(1);
  return info;
}

bool checkSIPMessage(QList<QStringList>& values)
{
  qDebug() << "Checking SIP message";
  if(values.size() == 0)
  {
    qDebug() << "Message creation had failed previously";
    return false;
  }

  if(values.size() < SIPMANDATORYLINES)
  {
    qDebug() << "Found SIP message with too few lines:" << values.size() << "lines";
    return false;
  }

  for(unsigned int i = 0; i < SIPHEADERLINES.size(); ++i)
  {
    if(values[i].length() != SIPHEADERLINES.at(i).words)
    {
      qDebug() << "Wrong number of words in a line for:" << SIPHEADERLINES.at(i).name
               << "(" << i << ")"
               <<". Found:" << values[i].length()
               << "Expecting:" << SIPHEADERLINES.at(i).words;
      qDebug() << "Line:" << values[i];
      return false;
    }
  }

  qDebug() << "No immidiate problems detected in SIP message";
  return true;
}

void parseSIPaddress(QString address, QString& user, QString& location)
{
  QStringList splitAddress = address.split("@");

  if(splitAddress.size() != 2)
  {
    user = "";
    location = "";
    return;
  }

  user = splitAddress.at(0).right(splitAddress.at(0).length() - 5);
  location = splitAddress.at(1).left(splitAddress.at(1).length() - 1);

  qDebug() << "Parsed SIP address:" << address
           << "to user:" << user << "and location:" << location;
}

bool parseSIPParameter(QString field, QString parameterName,
                       QString& parameterValue, QString& remaining)
{
  QStringList parameterSplit = field.split(parameterName);

  if(parameterSplit.size() != 2)
  {
    qDebug() << "Did not find" << parameterName << "in" << field;
    parameterValue = "";
  }
  else
  {
    qDebug() << "Found" << parameterName;
    parameterValue = parameterSplit.at(1);
  }
  remaining = parameterSplit.at(0);
}

std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body)
{
  QStringList lines = body.split("\r\n", QString::SkipEmptyParts);
  QStringListIterator lineIterator(lines);
  bool version = false;
  bool originator = false;
  bool session = false;
  bool timing = false;
  bool contact = false;

  std::shared_ptr<SDPMessageInfo> info(new SDPMessageInfo);

  while(lineIterator.hasNext())
  //for(QStringListIterator line = lines.iterator; line != lines.end(); ++line)
  //for(auto line: lines)
  {
    QString line = lineIterator.next();
    qDebug() << "Parsing SDP line:" << line;

    QStringList words = line.split(" ", QString::SkipEmptyParts);

    QString firstValue = "";

    switch(line.at(0).toLatin1())
    {
    case 'v':
    {

      if(!checkSDPLine(words, 1, firstValue))
        return NULL;

      info->version = firstValue.toUInt();
      if(info->version != 0)
      {
        qDebug() << "Unsupported SDP version:" << info->version;
        return NULL;
      }

      version = true;
      break;
    }
    case 'o':
    {
      if(!checkSDPLine(words, 6, info->username))
        return NULL;

      info->sess_id = words.at(1).toUInt();
      info->sess_v = words.at(2).toUInt();
      info->nettype = words.at(3);
      info->addrtype = words.at(4);
      info->hostAddress = words.at(5);

      qDebug() << "Origin read successfully. host_address:" << info->hostAddress;

      originator = true;
      break;
    }
    case 's':
    {
      if(line.size() < 3 )
        return NULL;

      info->sessionName = line.right(words.at(0).size() - 2);

      qDebug() << "Session name:" << info->sessionName;

      break;
    }
    case 't':
    {
      if(!checkSDPLine(words, 2, firstValue))
        return NULL;

      info->startTime = firstValue.toUInt();
      info->endTime = words.at(1).toUInt();

      break;
    }
    case 'm':
    {
      if( words.size() != 4)
        return NULL;
      break;
    }
    default:
    {
      qDebug() << "Unimplemented or malformed SDP line type:" << line;
    }
    }
  }

  if(!version || !originator || !session || timing)
  {
    qDebug() << "All required fields not present in SDP";
    //return NULL;
  }

  return info;
}
