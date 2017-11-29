#include "sipparser.h"

#include "common.h"

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
  quint8 minWords;
  quint8 maxWords;
  bool mandatory;
};

// TODO: possiblity of having more than one same line in header.
// TODO: Maybe make SIP implementation simpler?

// remember to also update tableToInfo if adding new header fields!
const QList<HeaderLine> SIPHEADERLINES(
{
      {"METHOD", 3, 4, true},
      {"Via:", 3, 3, true},
      {"Max-Forwards:", 2, 2, true},
      {"To:", 3, 3, true},
      {"From:", 3, 3, true},
      {"Call-ID:", 2, 2, true},
      {"Cseq:", 3, 3, true},
      {"Contact:", 2, 2, true},
      {"Content-Type:", 2, 2, false},
      {"Content-Length:", 2, 2, true},
}
);

// TODO this changes depending on the message type, maybe make a function/table for that
const uint16_t SIPMANDATORYLINES = 9;

// Local functions
void messageToTable(QStringList& lines, QList<QStringList> &values);
SIPMessageInfo* tableToInfo(QList<QStringList>& values);
bool checkSIPMessage(QList<QStringList>& values);

void parseSIPaddress(QString address, QString& user, QString& location);
void parseSIPParameter(QString field, QString parameterName,
                       QString& parameterValue, QString& remaining);

void cleanup(SIPMessageInfo* info);

bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue);

RequestType parseRequest(QString request);

// Implementations

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

RequestType parseRequest(QString request)
{
  if(request == "INVITE")
  {
    qDebug() << "INVITE found";
    return INVITE;
  }
  else if(request == "ACK")
  {
    qDebug() << "ACK found";
    return ACK;
  }
  else if(request == "BYE")
  {
    qDebug() << "BYE found";
    return BYE;
  }

  qDebug() << "No sensible SIP message found";
  return NOREQUEST;
}

void cleanup(SIPMessageInfo* info)
{
  if(info != 0)
    delete info;
}

std::shared_ptr<SIPMessageInfo> parseSIPMessage(QString& header)
{
  QStringList lines = header.split("\r\n",  QString::SkipEmptyParts);

  qDebug() << "SIP message split into"  << lines.length() << "lines";

  QList<QStringList> values;

  // increase the list size
  while(values.size() < SIPHEADERLINES.size())
  {
    values.append(QStringList());
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

  for(int32_t i = 0;  i < lines.length(); ++i)
  {
    QStringList words = lines.at(i).split(" ");

    if(i == 0)
    {
      values[0] = words;
    }
    else
    {
      // find headertype in array
      for(int32_t j = 1; j < SIPHEADERLINES.size(); ++j)
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

  // TODO: this affects which header-lines are mandatory,
  //       and it should be taken into account

  if(values.at(0).at(0) != "SIP/2.0")
  {
    info->request = parseRequest(values.at(0).at(0));
    if(info->request == NOREQUEST)
    {
      cleanup(info);
      return NULL;
    }
    info->response = NORESPONSE;
  }
  else
  {
    info->request = NOREQUEST;
    if(values.at(0).at(1) == "180")
    {
      qDebug() << "RINGING found";
      info->response = RINGING_180;
    }
    else if(values.at(0).at(1) == "200")
    {
      qDebug() << "OK found";
      info->response = OK_200;
    }
    else if(values.at(0).at(1) == "603")
    {
      qDebug() << "DECLINE found";
      info->response = DECLINE_603;
    }
    else
    {
      qDebug() << "Unrecognized response:" << values.at(0).at(1) << values.at(0).at(2);
      cleanup(info);
      return NULL;
    }
  }

  // values has been set according to HEADERLINES-table
  // and their existance has been checked in checkSIPMessage-function
  info->version = values.at(0).at(2).right(3);
  qDebug() << "Version:" << info->version;

  info->remoteName = values.at(4).at(1);
  info->localName = values.at(3).at(1);

  QString replyAddress = "";
  parseSIPParameter(values.at(1).at(2), ";branch=", info->branch, replyAddress);

  if(STRICTSIPPROCESSING && info->branch.isEmpty())
  {
    cleanup(info);
    return NULL;
  }

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

  parseSIPParameter(values.at(3).at(2), ";tag=", info->localTag, toSIPAddress);
  parseSIPParameter(values.at(4).at(2), ";tag=", info->remoteTag, fromSIPAddress);

  if(STRICTSIPPROCESSING && info->remoteTag.isEmpty())
  {
    qDebug() << "They did not send their tag!";
    cleanup(info);
    return NULL;
  }

  parseSIPaddress(toSIPAddress, info->localUsername, info->localLocation);
  parseSIPaddress(fromSIPAddress, info->remoteUsername, info->remoteLocation);

  bool ok = false;
  info->cSeq = values.at(6).at(1).toInt(&ok);

  info->originalRequest = parseRequest(values.at(6).at(2));

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

  if(values.size() >= 9 && values.at(8).size() == 2)
  {
    info->contentType = values.at(8).at(1);
  }
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

  for(int32_t i = 0; i < SIPHEADERLINES.size(); ++i)
  {
    if(!values[i].isEmpty())
    {
      if(values[i].length() < SIPHEADERLINES.at(i).minWords ||
         values[i].length() > SIPHEADERLINES.at(i).maxWords)
      {
        qDebug() << "Wrong number of words in a line for:" << SIPHEADERLINES.at(i).name
                 << "(" << i << ")"
                 <<". Found:" << values[i].length()
                << "Expecting:" << SIPHEADERLINES.at(i).minWords << "-" << SIPHEADERLINES.at(i).maxWords;
        qDebug() << "Line:" << values[i];
        return false;
      }
    }
    else if(SIPHEADERLINES.at(i).mandatory)
    {
      qDebug() << "SIP message missing a mandatory line:" << SIPHEADERLINES.at(i).name;
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
}

void parseSIPParameter(QString field, QString parameterName,
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
  std::shared_ptr<SDPMessageInfo> info(new SDPMessageInfo);
  bool version = false;
  bool originator = false;
  bool session = false;
  bool timing = false;
  bool globalContact = false;
  bool localContacts = true;

  QStringList lines = body.split("\r\n", QString::SkipEmptyParts);
  QStringListIterator lineIterator(lines);

  while(lineIterator.hasNext())
  {
    QString line = lineIterator.next();

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
        if(!checkSDPLine(words, 6, info->originator_username))
          return NULL;

        info->sess_id = words.at(1).toUInt();
        info->sess_v = words.at(2).toUInt();
        info->host_nettype = words.at(3);
        info->host_addrtype = words.at(4);
        info->host_address = words.at(5);

        qDebug() << "Origin read successfully. host_address:" << info->host_address;

        originator = true;
        break;
      }
      case 's':
      {
        if(line.size() < 3 )
          return NULL;

        info->sessionName = line.right(line.size() - 2);

        qDebug() << "Session name:" << info->sessionName;
        session = true;
        break;
      }
      case 't':
      {
        if(!checkSDPLine(words, 2, firstValue))
          return NULL;

        info->startTime = firstValue.toUInt();
        info->endTime = words.at(1).toUInt();

        timing = true;
        break;
      }
      case 'm':
      {
        MediaInfo mediaInfo;
        if(!checkSDPLine(words, 4, mediaInfo.type))
          return NULL;

        bool mediaContact = false;
        mediaInfo.receivePort = words.at(1).toUInt();
        mediaInfo.proto = words.at(2);
        mediaInfo.rtpNum = words.at(3).toUInt();

        qDebug() << "Media found with type:" << mediaInfo.type;

        // TODO process other possible lines
        while(lineIterator.hasNext()) // TODO ERROR if not correct, the line is not processsed, move backwards?
        {
          QString additionalLine = lineIterator.next();
          QStringList additionalWords = additionalLine.split(" ", QString::SkipEmptyParts);


          if(additionalLine.at(0) == 'c')
          {
            if(!checkSDPLine(additionalWords, 3, mediaInfo.nettype))
              return NULL;

            mediaInfo.addrtype = additionalWords.at(1);
            mediaInfo.address = additionalWords.at(2);

            info->connection_addrtype = additionalWords.at(1); // TODO: what about nettype?
            info->connection_address = additionalWords.at(2);

            mediaContact = true;
          }
          else if(additionalLine.at(0) == "a=rtpmap")
          {
            if(additionalWords.size() >= 2)
            {
              if(additionalWords.at(0).size() < 3)
              {
                qDebug() << "rtpmap missing first value";
                break;
              }

              QStringList payloadNum = additionalWords.at(0).split(":", QString::SkipEmptyParts);
              QStringList encoding = additionalWords.at(1).split("/", QString::SkipEmptyParts);

              if(payloadNum.size() != 2 || encoding.size() != 2)
                break;

              RTPMap mapping;
              mapping.rtpNum = payloadNum.at(1).toUInt();
              mapping.codec = encoding.at(0);
              mapping.clockFrequency = encoding.at(1).toUInt();

              qDebug() << "RTPmap found with codec:" << mapping.codec;

              mediaInfo.codecs.append(mapping);
            }
            else
            {
              qDebug() << "too few words in rtp map";
              break;
            }

            if(additionalWords.size() < 4)
            {
              qDebug() << "Detected unsupported encoding parameters.";
              // TODO support encoding parameters
            }
          }
          else
          {
            lineIterator.previous();
            break;
          }
        }

        info->media.append(mediaInfo);

        if(!mediaContact)
          localContacts = false;

        break;
      }
      case 'c':
      {
        if(!checkSDPLine(words, 3, info->connection_nettype))
          return NULL;

        info->connection_addrtype = words.at(1);
        info->connection_address = words.at(2);
        globalContact = true;
        break;
      }
      default:
      {
        qDebug() << "Unimplemented or malformed SDP line type:" << line;
      }
    }
  }

  if(!version || !originator || !session || !timing || (!globalContact && !localContacts))
  {
    qDebug() << "All required fields not present in SDP:"
             << "v" << version << "o" << originator << "s" << session << "t" << timing
             << "global c" << globalContact << "local c" << localContacts;
    //return NULL;
  }

  return info;
}


QList<QHostAddress> parseIPAddress(QString address)
{
  QList<QHostAddress> ipAddresses;

  QRegularExpression re("\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\.|$)){4}\\b");
  if(re.match(address).hasMatch())
  {
    qDebug() << "Found IPv4 address:" << address;
    ipAddresses.append(QHostAddress(address));
  }
  else
  {
    qDebug() << "Did not find IPv4 address:" << address;
    QHostInfo hostInfo;
    hostInfo.setHostName(address);

    ipAddresses.append(hostInfo.addresses());
  }
  return ipAddresses;
}
