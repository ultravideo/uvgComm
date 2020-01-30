#include "sipcontent.h"

#include "sdptypes.h"

#include "common.h"

#include <QDebug>
#include <QRegularExpression>
#include <QSettings>



// called for every new line in SDP parsing. Parses out the two first characters
// and gives the first value in first element of words, and the rest of the values are divided to separate words.
// The first character is recorded to lineType.
bool nextLine(QStringListIterator &lineIterator, QStringList& words, char& lineType);

// Some fields simply take all the fields and put them in value regardless of spaces.
// Called after nextLine for this situation.
void gatherLine(QString& target, QStringList& words);



// c=
bool parseConnection(QStringListIterator& lineIterator, char& type, QStringList& words,
                     QString& nettype, QString& addrtype, QString& address);

// b=
bool parseBitrate(QStringListIterator& lineIterator, char& type, QStringList& words,
                  QList<QString>& bitrates);

// k=
bool parseEncryptionKey(QStringListIterator& lineIterator, char& type, QStringList& words,
                        QString& key);
// a=
bool parseAttributes(QStringListIterator &lineIterator, char &type, QStringList& words,
                     QList<SDPAttributeType>& flags, QList<SDPAttribute>& values,
                     QList<RTPMap>& codecs, QList<std::shared_ptr<ICEInfo>>& candidates);

void parseFlagAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttributeType>& attributes);
void parseValueAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttribute> valueAttributes);
void parseRTPMap(QRegularExpressionMatch& match, QString secondWord, QList<RTPMap>& codecs);
bool parseICECandidate(QStringList& words, QList<std::shared_ptr<ICEInfo>>& candidates);

bool checkSDPValidity(const SDPMessageInfo &sdpInfo)
{
  if(sdpInfo.version != 0 ||
     sdpInfo.originator_username.isEmpty() ||
     sdpInfo.sessionName.isEmpty() ||
     sdpInfo.timeDescriptions.empty() ||
     sdpInfo.media.empty())
  {
    printDebug(DEBUG_PROGRAM_WARNING, "SipContent", "SDP is not valid",
              {"Version", "Originator", "Session Name", "Number of time descriptions", "Number of medias"},
              {QString(sdpInfo.version), sdpInfo.originator_username, sdpInfo.sessionName,
                QString::number(sdpInfo.timeDescriptions.size()), QString::number(sdpInfo.media.size())});

    return false;
  }

  if (sdpInfo.host_nettype.isEmpty() ||
      sdpInfo.host_addrtype.isEmpty() ||
      sdpInfo.host_address.isEmpty())
  {
    qDebug() << "SDP Host address is empty.";
    qDebug() << sdpInfo.host_nettype << " " << sdpInfo.host_addrtype << " " << sdpInfo.host_address;
    return false;
  }

  if (sdpInfo.connection_nettype.isEmpty() ||
      sdpInfo.connection_addrtype.isEmpty() ||
      sdpInfo.connection_address.isEmpty())
  {
    qDebug() << "No Global address in SDP";
    for (auto& media: sdpInfo.media)
    {
      if (media.connection_address.isEmpty() ||
          media.connection_addrtype.isEmpty() ||
          media.connection_address.isEmpty())
      {
        qDebug() << "Missing global and media address. The SDP is not good";
        return false;
      }
    }
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  if (settings.value("sip/ice").toInt() == 1 && sdpInfo.candidates.isEmpty())
  {
    qDebug() << "PEER ERROR: Didn't receive any ICE candidates!";
    return false;
  }

  return true;
}


QString composeSDPContent(const SDPMessageInfo &sdpInfo)
{
  Q_ASSERT(checkSDPValidity(sdpInfo));
  if(!checkSDPValidity(sdpInfo))
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPContent",  "Bad SDPInfo in string formation.");
    return "";
  }
  else
  {
    printDebug(DEBUG_NORMAL, "SIPContent",  "Parameter SDP is valid. Starting to compose to string.");
  }

  QString sdp = "";
  QString lineEnd = "\r\n";
  sdp += "v=" + QString::number(sdpInfo.version) + lineEnd;
  sdp += "o=" + sdpInfo.originator_username + " " + QString::number(sdpInfo.sess_id)  + " "
      + QString::number(sdpInfo.sess_v) + " " + sdpInfo.host_nettype + " "
      + sdpInfo.host_addrtype + " " + sdpInfo.host_address + lineEnd;

  sdp += "s=" + sdpInfo.sessionName + lineEnd;
  sdp += "c=" + sdpInfo.connection_nettype + " " + sdpInfo.connection_addrtype +
      + " " + sdpInfo.connection_address + " " + lineEnd;

  sdp += "t=" + QString::number(sdpInfo.timeDescriptions.at(0).startTime) + " "
      + QString::number(sdpInfo.timeDescriptions.at(0).stopTime) + lineEnd;

  for(auto& mediaStream : sdpInfo.media)
  {
    sdp += "m=" + mediaStream.type + " " + QString::number(mediaStream.receivePort)
        + " " + mediaStream.proto;

    if (mediaStream.rtpNums.empty())
    {
      printDebug(DEBUG_PROGRAM_ERROR, "SIPContent",
                 "There was no RTP num included in SDP media!");
      return "";
    }

    for(uint8_t rtpNum : mediaStream.rtpNums)
    {
      sdp += " " + QString::number(rtpNum);
    }
    sdp += lineEnd;

    if(!mediaStream.title.isEmpty())
    {
      sdp += "i=" + mediaStream.title;
    }

    if(!mediaStream.connection_nettype.isEmpty())
    {
      sdp += "c=" + mediaStream.connection_nettype + " " + mediaStream.connection_addrtype + " "
          + mediaStream.connection_address + lineEnd;
    }

    for (auto& bitrate: mediaStream.bitrate)
    {
      sdp += "b=" + bitrate + lineEnd;
    }

    if (!mediaStream.encryptionKey.isEmpty())
    {
      sdp += "k=" + mediaStream.encryptionKey;
    }

    for (auto& rtpmap : mediaStream.codecs)
    {
      sdp += "a=rtpmap:" + QString::number(rtpmap.rtpNum) + " "
          + rtpmap.codec + "/" + QString::number(rtpmap.clockFrequency) + lineEnd;
    }

    for (SDPAttributeType flag : mediaStream.flagAttributes)
    {
      switch (flag)
      {
      case A_SENDRECV:
      {
        sdp += "a=sendrecv"  + lineEnd;
        break;
      }
      case A_SENDONLY:
      {
        sdp += "a=sendonly"  + lineEnd;
        break;
      }
      case A_RECVONLY:
      {
        sdp += "a=recvonly"  + lineEnd;
        break;
      }
      case A_INACTIVE:
      {
        sdp += "a=inactive"  + lineEnd;
        break;
      }
      default:
      {
        qDebug() << "ERROR: Trying to compose SDP flag attribute with unimplemented flag";
        break;
      }
      }
    }
  }

  for (auto& info : sdpInfo.candidates)
  {
    sdp += "a=candidate:"
        + info->foundation + " " + QString::number(info->component) + " "
        + info->transport  + " " + QString::number(info->priority)  + " "
        + info->address    + " " + QString::number(info->port)      + " "
        + "typ " + info->type + lineEnd;
  }

  qDebug().noquote() << "Sending, SIPContent :" << "Composed SDP string:" << sdp;  return sdp;
}

bool nextLine(QStringListIterator& lineIterator, QStringList& words, char& lineType)
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
    words[0] = words.at(0).right(words.at(0).size() - 2);
    // TODO: print the line type somehow
    //qDebug().nospace().noquote() << lineType << ", ";
    return true;
  }

  return false;
}


void gatherLine(QString& target, QStringList& words)
{
  // a little bit clumsy, simply takes everything other than the letter and '='-mark
  target = "";
  for(int i = 0; i < words.size(); ++i)
  {
    target += words.at(i);
  }

}

bool parseSDPContent(const QString& content, SDPMessageInfo &sdp)
{
  // The SDP has strict ordering rules and the parsing follows those.
  QStringList lines = content.split("\r\n", QString::SkipEmptyParts);
  if(lines.size() > 1000)
  {
    qDebug() << "WHOAH: Got over a thousand lines of SDP. "
                "Not going to process this because of size: " << lines.size();
    return false;
  }

  QStringListIterator lineIterator(lines);
  QStringList words;
  char type = ' ';

  // the SDP must either have one global connection (c=)-field or each media must have its own.
  bool globalConnection = false;

  // TODO: Print all recognized lines on one line.

  // v=
  if(!nextLine(lineIterator, words, type))
  {
    qDebug() << "Empty SDP message";
    return false;
  }

  if(type != 'v' || words.size() != 1)
  {
    qDebug() << "First line malformed";
    return false;
  }

  sdp.version = static_cast<uint8_t>(words.at(0).toUInt());

  if(sdp.version != 0)
  {
    qDebug() << "Unsupported SDP version:" << sdp.version;
    return false;
  }

  // o=
  if(!nextLine(lineIterator, words, type))
  {
    qDebug() << "Only v= line present";
    return false;
  }

  if(type != 'o' || words.size() != 6)
  {
    qDebug() << "o= line malformed";
    return false;
  }

  sdp.originator_username = words.at(0);
  sdp.sess_id = words.at(1).toUInt();
  sdp.sess_v = words.at(2).toUInt();
  sdp.host_nettype = words.at(3);
  sdp.host_addrtype = words.at(4);
  sdp.host_address = words.at(5);

  // s=
  // TODO: accept single empty character
  if(!nextLine(lineIterator, words, type) || type != 's')
  {
    qDebug() << "Problem gettin s= line";
    return false;
  }

  gatherLine(sdp.sessionName, words);

  // i=,u=,e=,p=,c=,b= or t=
  if(!nextLine(lineIterator, words, type))
  {
    qDebug() << "SDP ended without all mandatory lines!";
    return false;
  }

  if(type == 'i')
  {
    gatherLine(sdp.sessionDescription, words);

    // u=,e=,p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      qDebug() << "Nothing after i=";
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
    sdp.uri = words.at(0);

    // e=,p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      qDebug() << "Nothing after u=";
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

    gatherLine(sdp.email, words);

    // p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      qDebug() << "Nothing after e=";
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

    gatherLine(sdp.phone, words);

    // c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      qDebug() << "Nothing after p=";
      return false;
    }
  }


  if(!parseConnection(lineIterator, type, words,
                      sdp.connection_nettype, sdp.connection_addrtype, sdp.connection_address))
  {
    qDebug() << "Failed to parse connection";
    return false;
  }

  // the connection field must be present in either global stage or one in each media.
  globalConnection = sdp.connection_address != "";

  if(!parseBitrate(lineIterator, type, words, sdp.bitrate) || !lineIterator.hasNext())
  {
    return false;
  }

  if(type!= 't')
  {
    qDebug() << "No timing field present in SDP.";
    return false;
  }

  int timeDescriptions = 0;
  while(type == 't')
  {
    if(words.size() != 2)
    {
      qDebug() << "Wrong size for time description.";
      return false;
    }

    ++timeDescriptions;

    sdp.timeDescriptions.push_back(TimeInfo{words.at(0).toUInt(), words.at(1).toUInt(),"","",{}});

    // r=, t=, z=, k=, a=, m= or nothing
    if(!nextLine(lineIterator, words, type))
    {
      // this could be the end since timeDescription is the last mandatory element.
      return true;
    }

    if(type == 'r')
    {
      // must have at least three values.
      if(words.size() < 3)
      {
        qDebug() << "Failed to parse repeat interval (r=) line";
        return false;
      }

      sdp.timeDescriptions.last().repeatInterval = words.at(0);
      sdp.timeDescriptions.last().activeDuration = words.at(1);
      sdp.timeDescriptions.last().offsets.push_back(words.at(2));

      for(int i = 3; i < words.size(); ++i)
      {
        sdp.timeDescriptions[timeDescriptions].offsets.push_back(words.at(i));
      }

      // t=, z=, k=, a=, m= or nothing
      if(!nextLine(lineIterator, words, type))
      {
        return true;
      }
    }
  }

  if(type == 'z')
  {
    if(words.size() >= 2)
    {
      qDebug() << "Failed to parse time offset (z=) line";
      return false;
    }

    sdp.timezoneOffsets.push_back(TimezoneInfo{words.at(0), words.at(1)});

    for(int i = 2; i + 1 < words.size(); i += 2)
    {
      sdp.timezoneOffsets.push_back(TimezoneInfo{words.at(i), words.at(i + 1)});
    }

    // k=, a=, m= or nothing
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }
  }

  if(!parseEncryptionKey(lineIterator, type, words, sdp.encryptionKey))
  {
    qDebug() << "Failed to parse encryption key.";
    return false;
  }

  QList<RTPMap> noCodecs;
  if(!parseAttributes(lineIterator, type, words, sdp.flagAttributes, sdp.valueAttributes, noCodecs, sdp.candidates))
  {
    qDebug() << "Failed to parse attributes";
    return false;
  }

  if(!noCodecs.empty())
  {
    qDebug() << "Found rtpmap outside media";
    return false;
  }

  while(type == 'm')
  {
    qDebug() << "Found media:" << words.at(0);
    if(words.size() < 4)
    {
      qDebug() << "Failed to parse media because its has too few words";
      return false;
    }

    sdp.media.push_back(MediaInfo{words.at(0), static_cast<uint16_t>(words.at(1).toUInt()), words.at(2),
                                  {}, "","","", "", {}, "", {}, {},{}});

    for(int i = 3; i < words.size(); ++i)
    {
      sdp.media.back().rtpNums.push_back(static_cast<uint8_t>(words.at(i).toUInt()));
    }

    // m=, i=, c=, b=,  or nothing.
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }

    if(type == 'i')
    {
      gatherLine(sdp.media.back().title, words);

      // u=,e=,p=,c=,b= or
      if(!nextLine(lineIterator, words, type))
      {
        return true;
      }
    }

    if(!globalConnection && type != 'c')
    {
      qDebug() << "Not all media have a connection field!";
      return false;
    }

    // parse c=, b=, k= and a= fields
    if(!parseConnection(lineIterator, type, words, sdp.media.back().connection_nettype,
                        sdp.media.back().connection_addrtype, sdp.media.back().connection_address)
       || !parseBitrate(lineIterator, type, words, sdp.media.back().bitrate)
       || !parseEncryptionKey(lineIterator, type, words, sdp.encryptionKey)
       || !parseAttributes(lineIterator, type, words,
                           sdp.media.back().flagAttributes,
                           sdp.media.back().valueAttributes,
                           sdp.media.back().codecs,
                           sdp.candidates))
    {
      qDebug() << "Failed to parse some media fields";
      return false;
    }

  } // m=

  if(!checkSDPValidity(sdp))
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPContent",
               "The parsing generated a bad SDP for some reason. The problem should be detected earlier.");
    return false;
  }
  else
  {
    qDebug() << "Parsed SDP is valid.";
  }

  return true;
}

bool parseAttributes(QStringListIterator &lineIterator, char &type, QStringList& words,
                     QList<SDPAttributeType>& flags, QList<SDPAttribute>& values,
                     QList<RTPMap>& codecs, QList<std::shared_ptr<ICEInfo>>& candidates)
{
  while(type == 'a')
  {
    // ignore non recognized attributes.

    QRegularExpression re_attribute("(\\w+)(:(\\S+))?");
    QRegularExpressionMatch match = re_attribute.match(words.at(0));
    if(match.hasMatch() && match.lastCapturedIndex() >= 1)
    {
      QString attribute = match.captured(1);

      std::map<QString, SDPAttributeType> xmap = {
             {"cat",       A_CAT},      {"keywds",   A_KEYWDS},   {"tool",      A_TOOL},
             {"maxptime",  A_MAXPTIME}, {"rtpmap",   A_RTPMAP},   {"recvonly",  A_RECVONLY},
             {"sendrecv",  A_SENDRECV}, {"sendonly", A_SENDONLY}, {"inactive",  A_INACTIVE},
             {"orient",    A_ORIENT},   {"type",     A_TYPE},     {"charset",   A_CHARSET},
             {"sdplang",   A_SDPLANG},  {"lang",     A_LANG},     {"framerate", A_FRAMERATE},
             {"quality",   A_QUALITY},  {"ptime",    A_PTIME},    {"fmtp",      A_FMTP},
             {"candidate", A_CANDIDATE}};

        if(xmap.find(attribute) != xmap.end())
        {
          switch(xmap[attribute])
          {
          case A_CAT:
          {
            parseValueAttribute(A_CAT, match, values);
            break;
          }
          case A_KEYWDS:
          {
            parseValueAttribute(A_KEYWDS, match, values);
            break;
          }
          case A_TOOL:
          {
            parseValueAttribute(A_TOOL, match, values);
            break;
          }
          case A_PTIME:
          {
            parseValueAttribute(A_PTIME, match, values);
            break;
          }
          case A_MAXPTIME:
          {
            parseValueAttribute(A_MAXPTIME, match, values);
            break;
          }
          case A_RTPMAP:
          {
            if(words.size() != 2)
            {
              qDebug() << "Wrong amount of words in rtpmap " << words.size() << "Expected 2";
              return false;
            }
            parseRTPMap(match, words.at(1), codecs);
            break;
          }
          case A_RECVONLY:
          {
            parseFlagAttribute(A_RECVONLY, match, flags);
            break;
          }
          case A_SENDRECV:
          {
            parseFlagAttribute(A_SENDRECV, match, flags);
            break;
          }
          case A_SENDONLY:
          {
            parseFlagAttribute(A_SENDONLY, match, flags);
            break;
          }
          case A_INACTIVE:
          {
            parseFlagAttribute(A_INACTIVE, match, flags);
            break;
          }
          case A_ORIENT:
          {
            parseValueAttribute(A_ORIENT, match, values);
            break;
          }
          case A_TYPE:
          {
            parseValueAttribute(A_TYPE, match, values);
            break;
          }
          case A_CHARSET:
          {
            parseValueAttribute(A_CHARSET, match, values);
            break;
          }
          case A_SDPLANG:
          {
            parseValueAttribute(A_SDPLANG, match, values);
            break;
          }
          case A_LANG:
          {
            parseValueAttribute(A_LANG, match, values);
            break;
          }
          case A_FRAMERATE:
          {
            parseValueAttribute(A_FRAMERATE, match, values);
            break;
          }
          case A_QUALITY:
          {
            parseValueAttribute(A_QUALITY, match, values);
            break;
          }
          case A_FMTP:
          {
            parseValueAttribute(A_FMTP, match, values);
            break;
          }
          case A_CANDIDATE:
          {
            parseICECandidate(words, candidates);
            break;
          }
          default:
          {
            qDebug() << "ERROR: Recognized SDP attribute type which is not implemented";
            break;
          }
          }
        }
        else {
          qDebug() << "Could not find the attribute.";
        }
    }
    else
    {
      qDebug() << "Failed to parse attribute because of an unknown format: " << words;
    }

    // TODO: Check that there are as many codecs as there are rtpnums

    // a=, m= or nothing
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }
  }

  return true;
}

void parseFlagAttribute(SDPAttributeType type, QRegularExpressionMatch& match, QList<SDPAttributeType>& attributes)
{
  if(match.lastCapturedIndex() == 1)
  {
    qDebug() << "Correctly matched a flag attribute: " << match.captured(0);
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


bool parseConnection(QStringListIterator& lineIterator, char& type, QStringList& words,
                     QString& nettype, QString& addrtype, QString& address)
{
  if(type == 'c')
  {
    if(words.size() != 3)
    {
      qDebug() << "wrong number of values in connection:" << words.size() << "Expected 3";
      return false;
    }

    nettype = words.at(0);
    addrtype = words.at(1);
    address = words.at(2);

    // b= or t= if global
    if(!nextLine(lineIterator, words, type))
    {
      return false;
    }
  }
  return true;
}


bool parseBitrate(QStringListIterator& lineIterator, char& type, QStringList& words,
                  QList<QString>& bitrates)
{
  while(type == 'b')
  {
    if(words.size() != 1)
    {
      qDebug() << "More than one value in bitrate.";
      return false;
    }

    bitrates.push_back(words.at(0));

    qDebug() << "WARNING: Received a bitrate field, which is currently unsupported by us.";

    // t= if global
    // k=, a=, m= or nothing if media.
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }
  }
  return true;
}


bool parseEncryptionKey(QStringListIterator& lineIterator, char& type, QStringList& words,
                        QString& key)
{
  if(type == 'k')
  {
    if(words.size() != 1)
    {
      qDebug() << "More than one value in encryption key.";
      return false;
    }

    key = words.at(0);

    qDebug() << "WARNING: Received a encryption key field, which is currently unsupported by us.";

    // a=, m= or nothing
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }
  }
  return true;
}

bool parseICECandidate(QStringList& words, QList<std::shared_ptr<ICEInfo>>& candidates)
{
  if (words.size() != 8)
  {
    return false;
  }

  std::shared_ptr<ICEInfo> candidate = std::make_shared<ICEInfo>();

  candidate->foundation  = words.at(0).split(":").at(1);
  candidate->component   = words.at(1).toInt();
  candidate->transport   = words.at(2);
  candidate->priority    = words.at(3).toInt();
  candidate->address     = words.at(4);
  candidate->port        = words.at(5).toInt();
  candidate->type        = words.at(7); // discard word 6 (typ)
  candidate->rel_address = "";

  candidates.push_back(candidate);

  return true;
}
