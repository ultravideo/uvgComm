#include "sipcontent.h"

#include "sip/sdptypes.h"

#include <QRegularExpression>
#include <QDebug>


// called for every new line in SDP parsing. Parses out the two first characters and gives the first value,
// and the rest of the values are divided to separate words. The amount of words can be checked and
// the first character is recorded to lineType.
bool nextLine(QStringListIterator &lineIterator, QStringList& words, char& lineType,
              QString& firstValue);

// Some fields simply take all the fields and put them in value regardless of spaces.
// Called after nextLine for this situation.
void gatherLine(QString& target, QString firstValue, QStringList& words);

void parseFlagAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttributeType>& attributes);
void parseValueAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttribute> valueAttributes);
void parseRTPMap(QRegularExpressionMatch& match, QString secondWord, QList<RTPMap>& codecs);


QString composeSDPContent(const SDPMessageInfo &sdpInfo)
{
  if(sdpInfo.version != 0 ||
     sdpInfo.originator_username.isEmpty() ||
     sdpInfo.host_nettype.isEmpty() ||
     sdpInfo.host_addrtype.isEmpty() ||
     sdpInfo.host_address.isEmpty() ||
     sdpInfo.sessionName.isEmpty() ||
     sdpInfo.connection_nettype.isEmpty() ||
     sdpInfo.connection_addrtype.isEmpty() ||
     sdpInfo.connection_address.isEmpty() ||
     sdpInfo.timeDescriptions.empty() ||
     sdpInfo.media.empty())
  {
    qCritical() << "WARNING: Bad SDPInfo in string formation";
    qWarning() << sdpInfo.version <<
        sdpInfo.originator_username <<
        sdpInfo.host_nettype <<
        sdpInfo.host_addrtype <<
        sdpInfo.host_address <<
        sdpInfo.sessionName <<
        sdpInfo.connection_nettype <<
        sdpInfo.connection_addrtype <<
        sdpInfo.connection_address <<
        sdpInfo.timeDescriptions.size();
    return "";
  }
  QString sdp = "";
  QString lineEnd = "\r\n";
  sdp += "v=" + QString::number(sdpInfo.version) + lineEnd;
  sdp += "o=" + sdpInfo.originator_username + " " + QString::number(sdpInfo.sess_id)  + " "
      + QString::number(sdpInfo.sess_v) + " " + sdpInfo.connection_nettype + " "
      + sdpInfo.connection_addrtype + " " + sdpInfo.connection_address + lineEnd;

  sdp += "s=" + sdpInfo.sessionName + lineEnd;
  sdp += "c=" + sdpInfo.connection_nettype + " " + sdpInfo.connection_addrtype +
      + " " + sdpInfo.connection_address + " " + lineEnd;

  sdp += "t=" + QString::number(sdpInfo.timeDescriptions.at(0).startTime) + " "
      + QString::number(sdpInfo.timeDescriptions.at(0).stopTime) + lineEnd;

  for(auto mediaStream : sdpInfo.media)
  {
    sdp += "m=" + mediaStream.type + " " + QString::number(mediaStream.receivePort)
        + " " + mediaStream.proto;

    for(uint8_t rtpNum : mediaStream.rtpNums)
    {
      sdp += " " + QString::number(rtpNum);
    }
    sdp += lineEnd;

    if(!mediaStream.connection_nettype.isEmpty())
    {
      sdp += "c=" + mediaStream.connection_nettype + " " + mediaStream.connection_addrtype + " "
          + mediaStream.connection_address + lineEnd;
    }

    for(auto rtpmap : mediaStream.codecs)
    {
      sdp += "a=rtpmap:" + QString::number(rtpmap.rtpNum) + " "
          + rtpmap.codec + "/" + QString::number(rtpmap.clockFrequency) + lineEnd;
    }
  }

  qDebug().noquote() << "Composed SDP string:" << sdp;
  return sdp;
}

bool nextLine(QStringListIterator& lineIterator, QStringList& words, char& lineType,
              QString& firstValue)
{
  if(lineIterator.hasNext())
  {
    QString line = lineIterator.next();
    words = line.split(" ", QString::SkipEmptyParts);

    if(words.at(0).length() < 3)
    {
      qDebug() << "First word missing in SDP Line:" << line;
      return false;
    }

    lineType = words.at(0).at(0).toLatin1();

    // skip first two characters of first word. (for example "v=")
    firstValue = words.at(0).right(words.at(0).size() - 2);

    qDebug() << "Found: " << lineType << " SDP field";
    return true;
  }

  return false;
}


void gatherLine(QString& target, QString firstValue, QStringList& words)
{
  // a little bit clumsy, simply takes everything other than the letter and '='-mark
  target = firstValue;
  for(unsigned int i = 1; i < words.size(); ++i)
  {
    target += words.at(i);
  }

}


bool parseSDPContent(const QString& content, SDPMessageInfo &sdp)
{
  // The SDP has strict ordering rules and the parsing follows those.
  QStringList lines = content.split("\r\n", QString::SkipEmptyParts);
  QStringListIterator lineIterator(lines);
  QStringList words;
  char type = ' ';
  QString firstValue = "";

  // the SDP must either have one global connection (c=)-field or each media must have its own.
  bool globalConnection = false;

  // v=
  if(!nextLine(lineIterator, words, type, firstValue))
  {
    return false;
  }

  if(type != 'v' || words.size() != 1)
  {
    return false;
  }

  sdp.version = firstValue.toUInt();

  if(sdp.version != 0)
  {
    qDebug() << "Unsupported SDP version:" << sdp.version;
    return false;
  }

  // o=
  if(!nextLine(lineIterator, words, type, firstValue))
  {
    return false;
  }

  if(type != 'o' || words.size() != 6)
  {
    return false;
  }

  sdp.originator_username = firstValue;
  sdp.sess_id = words.at(1).toUInt();
  sdp.sess_v = words.at(2).toUInt();
  sdp.connection_nettype = words.at(3);
  sdp.connection_addrtype = words.at(4);
  sdp.connection_address = words.at(5);

  // s=
  // TODO: accept single empty character
  if(!nextLine(lineIterator, words, type, firstValue) || type != 's')
  {
    return false;
  }

  gatherLine(sdp.sessionName, firstValue, words);

  // i=,u=,e=,p=,c=,b= or t=
  if(!nextLine(lineIterator, words, type, firstValue))
  {
    return false;
  }

  if(type == 'i')
  {
    gatherLine(sdp.sessionDescription, firstValue, words);

    // u=,e=,p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return false;
    }
  }

  if(type == 'u')
  {
    if(words.size() != 1)
    {
      qDebug() << "SDP URI size is wrong:" << words.size();
      return false;
    }

    // there should be no spaces in URI.
    sdp.uri = firstValue;

    // e=,p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return false;
    }
  }

  if(type == 'e')
  {
    if(words.size() > 4) // not sure if middle name is allowed. Should check
    {
      qDebug() << "Email field had too many words";
      return false;
    }

    gatherLine(sdp.email, firstValue, words);

    // p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return false;
    }
  }

  if(type == 'p')
  {
    if(words.size() > 6)
    {
      qDebug() << "Too many words in phone number. Phone number should be at most in 4 pieces + name.";
      return false;
    }

    gatherLine(sdp.phone, firstValue, words);

    // c=,b= or t=
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return false;
    }
  }

  if(type == 'c')
  {
    if(words.size() != 3)
    {
      return false;
    }

    sdp.connection_nettype = firstValue;
    sdp.connection_addrtype = words.at(1);
    sdp.connection_address = words.at(2);
    globalConnection = true;

    // b= or t=
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return false;
    }
  }

  while(type == 'b')
  {
    if(words.size() != 1)
    {
      return false;
    }

    sdp.bitrate.push_back(firstValue);

    qDebug() << "WARNING: Received a bitrate field, which is currently unsupported by us.";

    // t=
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return false;
    }
  }

  if(type!= 't' || words.size() != 2)
  {
    qDebug() << "No timing field present in SDP.";
    return false;
  }

  int timeDescriptions = 0;
  while(type == 't')
  {
    ++timeDescriptions;

    sdp.timeDescriptions.push_back(TimeInfo{firstValue.toUInt(), words.at(1).toUInt(),"","",{}});

    // r=, t=, z=, k=, a=, m= or nothing
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      // this could be the end since timeDescription is the last mandatory element.
      return true;
    }

    if(type == 'r')
    {
      // must have at least three values.
      if(words.size() < 3)
      {
        return false;
      }

      sdp.timeDescriptions.last().repeatInterval = firstValue;
      sdp.timeDescriptions.last().activeDuration = words.at(1);
      sdp.timeDescriptions.last().offsets.push_back(words.at(2));

      for(int i = 3; i < words.size(); ++i)
      {
        sdp.timeDescriptions[timeDescriptions].offsets.push_back(words.at(i));
      }

      // t=, z=, k=, a=, m= or nothing
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return true;
      }
    }
  }

  if(type == 'z')
  {
    if(words.size() >= 2)
    {
      return false;
    }

    sdp.timezoneOffsets.push_back(TimezoneInfo{firstValue, words.at(1)});

    for(int i = 2; i + 1 < words.size(); i += 2)
    {
      sdp.timezoneOffsets.push_back(TimezoneInfo{words.at(i), words.at(i + 1)});
    }

    // k=, a=, m= or nothing
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return true;
    }
  }

  if(type == 'k')
  {
    if(words.size() != 1)
    {
      return false;
    }

    sdp.encryptionKey = firstValue;

    // a=, m= or nothing
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return true;
    }
  }

  while(type == 'a')
  {
    if(words.size() != 1)
    {
      return false;
    }

    sdp.attributes.push_back(firstValue);

    // a=, m= or nothing.
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return true;
    }
  }

  while(type == 'm')
  {
    if(words.size() < 4)
    {
      return false;
    }

    sdp.media.push_back(MediaInfo{firstValue, static_cast<uint16_t>(words.at(1).toUInt()), words.at(2),
                                  {}, "","","", "", {}, "", {}, {},{}});

    for(int i = 3; i < words.size(); ++i)
    {
      sdp.media.back().rtpNums.push_back(static_cast<uint8_t>(words.at(i).toUInt()));
    }

    // m= or nothing.
    if(!nextLine(lineIterator, words, type, firstValue))
    {
      return true;
    }

    if(type == 'i')
    {
      gatherLine(sdp.media.back().title, firstValue, words);

      // u=,e=,p=,c=,b= or t=
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return true;
      }
    }

    if(type == 'c')
    {
      if(words.size() != 3)
      {
        return false;
      }

      sdp.media.back().connection_nettype = firstValue;
      sdp.media.back().connection_addrtype = words.at(1);
      sdp.media.back().connection_address = words.at(2);
      globalConnection = true;

      // b= or t=
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return false;
      }
    }

    while(type == 'b')
    {
      if(words.size() != 1)
      {
        return false;
      }

      sdp.media.back().bitrate.push_back(firstValue);

      qDebug() << "WARNING: Received a bitrate field, which is currently unsupported by us.";

      // t=
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return false;
      }
    }

    if(type == 'k')
    {
      if(words.size() != 1)
      {
        return false;
      }

      sdp.media.back().encryptionKey = firstValue;

      // a=, m= or nothing
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return true;
      }
    }

    while(type == 'a')
    {
      // ignore non recognized attributes.

      QRegularExpression re_attribute("(\\w+)(:(\\S+))?");
      QRegularExpressionMatch match = re_attribute.match(firstValue);
      if(match.hasMatch() && match.lastCapturedIndex() >= 2)
      {
        QString attribute = match.captured(1);

        std::map<QString, SDPAttributeType> xmap
            = {{"cat", A_CAT}, {"keywds", A_KEYWDS},{"tool", A_TOOL},{"ptime", A_PTIME},
               {"maxptime", A_MAXPTIME},{"rtpmap", A_RTPMAP},{"recvonly", A_RECVONLY},
               {"sendrecv", A_SENDRECV},{"sendonly", A_SENDONLY},{"inactive", A_INACTIVE},
               {"orient", A_ORIENT},{"type", A_TYPE},{"charset", A_CHARSET},{"sdplang", A_SDPLANG},
               {"lang", A_LANG},{"framerate", A_FRAMERATE},{"quality", A_QUALITY},{"fmtp", A_FMTP}};

          if(xmap.find(attribute) == xmap.end())
          {
            qDebug() << "Could not find the attribute.";
          }
          switch(xmap[attribute])
          {
          case A_CAT:
          {
            parseValueAttribute(A_CAT, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_KEYWDS:
          {
            parseValueAttribute(A_KEYWDS, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_TOOL:
          {
            parseValueAttribute(A_TOOL, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_PTIME:
          {
            parseValueAttribute(A_PTIME, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_MAXPTIME:
          {
            parseValueAttribute(A_MAXPTIME, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_RTPMAP:
          {
            if(words.size() != 2)
            {
              qDebug() << "Wrong amount of words in rtpmap " << words.size();
              return false;
            }
            parseRTPMap(match, words.at(1), sdp.media.back().codecs);
            break;
          }
          case A_RECVONLY:
          {
            parseFlagAttribute(A_RECVONLY, match, sdp.media.back().flagAttributes);
            break;
          }
          case A_SENDRECV:
          {
            parseFlagAttribute(A_RECVONLY, match, sdp.media.back().flagAttributes);
            break;
          }
          case A_SENDONLY:
          {
            parseFlagAttribute(A_RECVONLY, match, sdp.media.back().flagAttributes);
            break;
          }
          case A_INACTIVE:
          {
            parseFlagAttribute(A_RECVONLY, match, sdp.media.back().flagAttributes);
            break;
          }
          case A_ORIENT:
          {
            parseValueAttribute(A_ORIENT, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_TYPE:
          {
            parseValueAttribute(A_TYPE, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_CHARSET:
          {
            parseValueAttribute(A_CHARSET, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_SDPLANG:
          {
            parseValueAttribute(A_SDPLANG, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_LANG:
          {
            parseValueAttribute(A_LANG, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_FRAMERATE:
          {
            parseValueAttribute(A_FRAMERATE, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_QUALITY:
          {
            parseValueAttribute(A_QUALITY, match, sdp.media.back().valueAttributes);
            break;
          }
          case A_FMTP:
          {
            parseValueAttribute(A_FMTP, match, sdp.media.back().valueAttributes);
            break;
          }
          default:
          {
            qDebug() << "ERROR: Recognized SDP attribute type which is not implemented";
            break;
          }
          }


        sdp.attributes.push_back(firstValue);
      }
      else
      {
        qDebug() << "Failed to parse attribute because of an unkown format: " << words;
      }

      // TODO: Check that there are as many codecs as there are rtpnums

      // a=, m= or nothing.
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return true;
      }
    }
  }

  return true;
}

void parseFlagAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttributeType>& attributes)
{
  if(match.lastCapturedIndex() == 2)
  {
    qDebug() << "Correctly matched a flag attribute";
    attributes.push_back(type);
  }
  else
  {
    qDebug() << "Flag attribute did not match correctly";
  }
}

void parseValueAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttribute> valueAttributes)
{
  if(match.lastCapturedIndex() == 3)
  {
    qDebug() << "Correctly matched an SDP value attribute";
    QString value = match.captured(2);
    valueAttributes.push_back(SDPAttribute{type, value});
  }
  else
  {
    qDebug() << "Value attribute did not match correctly";
  }
}

void parseRTPMap(QRegularExpressionMatch& match, QString secondWord, QList<RTPMap>& codecs)
{
  if(match.hasMatch() && secondWord != "" && match.lastCapturedIndex() == 3)
  {
    QRegularExpression re_rtpParameters("(\\w+)\\/(\\w+)(\\/\\w+)?");
    QRegularExpressionMatch parameter_match = re_rtpParameters.match(secondWord);
    if(parameter_match.hasMatch() && (parameter_match.lastCapturedIndex() == 2 || parameter_match.lastCapturedIndex() == 3))
    {
      codecs.push_back(RTPMap{static_cast<uint8_t>(match.captured(3).toUInt()),
                              parameter_match.captured(2).toUInt(), parameter_match.captured(1), ""});
      if(parameter_match.lastCapturedIndex() == 3) // has codec parameters
      {
        codecs.back().codecParameter = parameter_match.captured(3);
      }
    }
    else
    {
      qDebug() << "The second part in RTPMap did not match correctly for:" << secondWord
               << "Got" << parameter_match.lastCapturedIndex() << "Expected 3 or 4";
    }
  }
  else
  {
    qDebug() << "The first part of RTPMap did not match correctly:" << match.lastCapturedIndex() << "Expected: 4";
  }
}
