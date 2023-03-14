#include "sipcontent.h"

#include "sdptypes.h"

#include "logger.h"

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
bool parseFlagAttribute(SDPAttributeType type, QString value, QList<SDPAttributeType>& attributes);
bool parseValueAttribute(SDPAttributeType type, QString value, QList<SDPAttribute>& valueAttributes);
bool parseMediaAtributes(QStringListIterator &lineIterator, char &type, QStringList& words,
                         QList<SDPAttributeType> &flags, QList<SDPAttribute> &parsedValues,
                         QList<RTPMap> &parsedCodecs, QList<std::shared_ptr<ICEInfo> > &candidates);
bool parseSessionAtributes(QStringListIterator &lineIterator, char &type, QStringList& words,
                           QList<SDPAttributeType> &flags, QList<SDPAttribute> &parsedValues,
                           QList<MediaGroup> &groupings);

void parseRTPMap(QString value, QString secondWord, QList<RTPMap>& codecs);
void parseGroup(QString value, QStringList& words, QList<MediaGroup>& groups);
bool parseICECandidate(QStringList& words, QList<std::shared_ptr<ICEInfo>>& candidates);

bool parseAttributeTypeValue(QString word, SDPAttributeType& type, QString &value);

SDPAttributeType stringToAttributeType(QString attribute);
GroupType stringToGroupType(QString group);

bool checkSDPValidity(const SDPMessageInfo &sdpInfo)
{
  Logger::getLogger()->printNormal("SipContent", "Checking SDP validity");

  if(sdpInfo.version != 0 ||
     sdpInfo.originator_username.isEmpty() ||
     sdpInfo.sessionName.isEmpty() ||
     sdpInfo.timeDescriptions.empty() ||
     sdpInfo.media.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_WARNING, "SipContent", 
                                    "SDP is not valid",
                                    {"Version", "Originator", "Session Name", 
                                     "Number of time descriptions", "Number of medias"},
                                    {QString::number(sdpInfo.version), sdpInfo.originator_username,
                                     sdpInfo.sessionName, QString::number(sdpInfo.timeDescriptions.size()), 
                                     QString::number(sdpInfo.media.size())});

    return false;
  }

  if (sdpInfo.host_nettype.isEmpty() ||
      sdpInfo.host_addrtype.isEmpty() ||
      sdpInfo.host_address.isEmpty())
  {
    Logger::getLogger()->printError("SipContent", "SDP Host address is empty");
    Logger::getLogger()->printError("SipContent", sdpInfo.host_nettype + " " + sdpInfo.host_addrtype + " " + sdpInfo.host_address);

    return false;
  }

  if (sdpInfo.connection_nettype.isEmpty() ||
      sdpInfo.connection_addrtype.isEmpty() ||
      sdpInfo.connection_address.isEmpty())
  {
    Logger::getLogger()->printError("SipContent", "No Global address in SDP");
    for (auto& media: sdpInfo.media)
    {
      if (media.connection_address.isEmpty() ||
          media.connection_addrtype.isEmpty() ||
          media.connection_address.isEmpty())
      {
        Logger::getLogger()->printError("SipContent", "Missing global and media address. The SDP is not good");
        return false;
      }

      if (media.candidates.isEmpty())
      {
        Logger::getLogger()->printError("SipContent", "Didn't receive any ICE candidates for media!");
        return false;
      }
    }
  }

  return true;
}


QString composeSDPContent(const SDPMessageInfo &sdpInfo)
{
  Q_ASSERT(checkSDPValidity(sdpInfo));
  if(!checkSDPValidity(sdpInfo))
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SIPContent",  
                                    "Bad SDPInfo in string formation.");
    return "";
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SIPContent",  
                                    "Parameter SDP is valid. Starting to compose to string.");
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
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SIPContent",
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
        case A_NO_ATTRIBUTE:
        {
          break;
        }
        default:
        {
          Logger::getLogger()->printProgramError("SipContent",
                                                 "Trying to compose SDP flag attribute with unimplemented flag");
          break;
        }
      }
    }

    for (auto& info : mediaStream.candidates)
    {
      sdp += "a=candidate:"
          + info->foundation + " " + QString::number(info->component) + " "
          + info->transport  + " " + QString::number(info->priority)  + " "
          + info->address    + " " + QString::number(info->port)      + " "
          + "typ " + info->type;

      if (info->rel_address != "" && info->rel_port != 0)
      {
        sdp += " raddr " + info->rel_address +
            " rport " + QString::number(info->rel_port);
      }

      sdp += lineEnd;
    }
  }



  return sdp;
}

bool nextLine(QStringListIterator& lineIterator, QStringList& words, char& lineType)
{
  if(lineIterator.hasNext())
  {
    QString line = lineIterator.next();

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    words = line.split(" ", QString::SkipEmptyParts);
#else
    words = line.split(" ", Qt::SkipEmptyParts);
#endif

    if(words.at(0).length() < 3)
    {
      Logger::getLogger()->printError("SipContent", "SDP Line doesn't have enough words!");
      return false;
    }

    lineType = words.at(0).at(0).toLatin1();

    // skip first two characters of first word. (for example "v=")
    words[0] = words.at(0).right(words.at(0).size() - 2);
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
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QStringList lines = content.split("\r\n", QString::SkipEmptyParts);
#else
    QStringList lines = content.split("\r\n", Qt::SkipEmptyParts);
#endif

  if(lines.size() > 1000)
  {
    Logger::getLogger()->printError("SipContent", "Got over a thousand lines of SDP! "
                                                  "Not going to process this because of the size.");
    return false;
  }

  QStringListIterator lineIterator(lines);
  QStringList words;
  char type = ' ';

  // the SDP must either have one global connection (c=)-field or each media must have its own.
  bool globalConnection = false;

  // v=
  if(!nextLine(lineIterator, words, type))
  {
    Logger::getLogger()->printError("SipContent", "Empty SDP message!");
    return false;
  }

  if(type != 'v' || words.size() != 1)
  {
    Logger::getLogger()->printError("SipContent", "First line malformed");
    return false;
  }

  sdp.version = static_cast<uint8_t>(words.at(0).toUInt());

  if(sdp.version != 0)
  {
    Logger::getLogger()->printError("SipContent", "Unsupported SDP version", "version", QString::number(sdp.version));
    return false;
  }

  // o=
  if(!nextLine(lineIterator, words, type))
  {
    Logger::getLogger()->printError("SipContent", "Only v= line present");
    return false;
  }

  if(type != 'o' || words.size() != 6)
  {
    Logger::getLogger()->printError("SipContent", "o= line malformed");
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
    Logger::getLogger()->printError("SipContent", "Problem gettin s= line");
    return false;
  }

  gatherLine(sdp.sessionName, words);

  // i=,u=,e=,p=,c=,b= or t=
  if(!nextLine(lineIterator, words, type))
  {
    Logger::getLogger()->printError("SipContent", "SDP ended without all mandatory lines!");
    return false;
  }

  if(type == 'i')
  {
    gatherLine(sdp.sessionDescription, words);

    // u=,e=,p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      Logger::getLogger()->printError("SipContent", "Nothing after i=");
      return false;
    }
  }

  if(type == 'u')
  {
    if(words.size() != 1)
    {
      Logger::getLogger()->printError("SipContent", "SDP URI size is wrong", "URI words",
                                      QString::number(words.size()));
      return false;
    }

    // there should be no spaces in URI.
    sdp.uri = words.at(0);

    // e=,p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      Logger::getLogger()->printError("SipContent", "Nothing after u=");
      return false;
    }
  }

  if(type == 'e')
  {
    if(words.size() > 4) // not sure if middle name is allowed. Should check
    {
      Logger::getLogger()->printError("SipContent", "Email field had too many words");
      return false;
    }

    gatherLine(sdp.email, words);

    // p=,c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      Logger::getLogger()->printError("SipContent", "Nothing after e=");
      return false;
    }
  }

  if(type == 'p')
  {
    if(words.size() > 6)
    {
      Logger::getLogger()->printError("SipContent", "Too many words in phone number. "
                                                    "Phone number should be at most in 4 pieces + name.");
      return false;
    }

    gatherLine(sdp.phone, words);

    // c=,b= or t=
    if(!nextLine(lineIterator, words, type))
    {
      Logger::getLogger()->printError("SipContent", "Nothing after p=");
      return false;
    }
  }


  if(!parseConnection(lineIterator, type, words,
                      sdp.connection_nettype, sdp.connection_addrtype, sdp.connection_address))
  {
    Logger::getLogger()->printError("SipContent", "Failed to parse connection");
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
    Logger::getLogger()->printError("SipContent", "No timing field present in SDP");
    return false;
  }

  int timeDescriptions = 0;
  while(type == 't')
  {
    if(words.size() != 2)
    {
      Logger::getLogger()->printError("SipContent", "Wrong size for time description");
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
        Logger::getLogger()->printError("SipContent", "Failed to parse repeat interval (r=) line");
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
      Logger::getLogger()->printError("SipContent", "Failed to parse time offset (z=) line");
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
    Logger::getLogger()->printError("SipContent", "Failed to parse encryption key");
    return false;
  }

  if(!parseSessionAtributes(lineIterator, type, words, sdp.flagAttributes, sdp.valueAttributes, sdp.groupings))
  {
    Logger::getLogger()->printError("SipContent", "Failed to parse attributes");
    return false;
  }

  while(type == 'm')
  {
    Logger::getLogger()->printNormal("SipContent", "Found media", "media", words.at(0));
    if(words.size() < 4)
    {
      Logger::getLogger()->printError("SipContent", "Failed to parse media because its has too few words");
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
      Logger::getLogger()->printError("SipContent", "Not all media have a connection field!");
      return false;
    }

    // parse c=, b=, k= and a= fields
    if(!parseConnection(lineIterator, type, words, sdp.media.back().connection_nettype,
                        sdp.media.back().connection_addrtype, sdp.media.back().connection_address)
       || !parseBitrate(lineIterator, type, words, sdp.media.back().bitrate)
       || !parseEncryptionKey(lineIterator, type, words, sdp.encryptionKey)
       || !parseMediaAtributes(lineIterator, type, words,
                               sdp.media.back().flagAttributes,
                               sdp.media.back().valueAttributes,
                               sdp.media.back().codecs,
                               sdp.media.back().candidates))
    {
      Logger::getLogger()->printError("SipContent", "Failed to parse some media fields");
      return false;
    }

  } // m=

  if(!checkSDPValidity(sdp))
  {
    Logger::getLogger()->printError("SIPContent",
                                    "The parsing generated a bad SDP for some reason. "
                                    "The problem should be detected earlier.");
    return false;
  }
  else
  {
    Logger::getLogger()->printNormal("SipContent", "Parsed SDP is valid");
  }

  return true;
}


bool parseMediaAtributes(QStringListIterator &lineIterator, char &type, QStringList& words,
                         QList<SDPAttributeType>& flags, QList<SDPAttribute>& parsedValues,
                         QList<RTPMap>& parsedCodecs, QList<std::shared_ptr<ICEInfo>>& candidates)
{
  while(type == 'a')
  {
    SDPAttributeType attribute = A_NO_ATTRIBUTE;
    QString value = "";
    if (!parseAttributeTypeValue(words.at(0), attribute, value))
    {
      Logger::getLogger()->printError("SIPContent", "Failed to parse session attribute");
      return false;
    }

    switch(attribute)
    {
      case A_PTIME:
      case A_MAXPTIME:
      case A_ORIENT:
      case A_SDPLANG:
      case A_LANG:
      case A_FRAMERATE:
      case A_QUALITY:
      case A_MID: // media stream identification, see RFC 5888
      {
        parseValueAttribute(attribute, value, parsedValues);
        break;
      }
      case A_RECVONLY:
      case A_SENDRECV:
      case A_SENDONLY:
      case A_INACTIVE:
      {
        parseFlagAttribute(attribute, value, flags);
        break;
      }
      case A_RTPMAP:
      {
        if(words.size() != 2)
        {
          Logger::getLogger()->printError("SipContent", "Wrong amount of words in rtpmap, expected 2",
                                          "words", QString::number(words.size()));
          return false;
        }
        parseRTPMap(value, words.at(1), parsedCodecs);
        break;
      }
      case A_FMTP:
      {
        // TODO
        break;
      }
      case A_CANDIDATE: // RFC 8445
      {
        parseICECandidate(words, candidates);
        break;
      }
      default:
      {
        Logger::getLogger()->printWarning("SIPContent", "Unrecognized media attribute, ignoring",
                                          "Atrribute", words.at(0));
      }
    }

    // a=, m= or nothing
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }
  }

  return true;
}


bool parseSessionAtributes(QStringListIterator &lineIterator, char &type, QStringList& words,
                           QList<SDPAttributeType>& flags, QList<SDPAttribute>& parsedValues,
                           QList<MediaGroup>& groupings)
{
  while(type == 'a')
  {
    SDPAttributeType attribute = A_NO_ATTRIBUTE;
    QString value = "";

    if (!parseAttributeTypeValue(words.at(0), attribute, value))
    {
      Logger::getLogger()->printError("SIPContent", "Failed to parse session attribute");
      return false;
    }
    switch(attribute)
    {
      case A_CAT:
      case A_KEYWDS:
      case A_TOOL:
      case A_TYPE:
      case A_CHARSET:
      case A_SDPLANG:
      case A_LANG:
      {
        parseValueAttribute(attribute, value, parsedValues);
        break;
      }
      case A_RECVONLY:
      case A_SENDRECV:
      case A_SENDONLY:
      case A_INACTIVE:
      {
        parseFlagAttribute(attribute, value, flags);
        break;
      }
      case A_GROUP: // RFC 5888
      {
        parseGroup(value, words, groupings);
        break;
      }
      default:
      {
        Logger::getLogger()->printWarning("SIPContent", "Unrecognized session attribute, ignoring",
                                          "Atrribute", words.at(0));
      }
    }

    // a=, m= or nothing
    if(!nextLine(lineIterator, words, type))
    {
      return true;
    }
  }

  return true;
}


bool parseFlagAttribute(SDPAttributeType type, QString value, QList<SDPAttributeType>& attributes)
{
  if(value == "")
  {
    Logger::getLogger()->printNormal("SipContent", "Correctly matched a flag attribute");
    attributes.push_back(type);
    return true;
  }

  Logger::getLogger()->printError("SipContent", "Flag attribute should not have value",
                                  "Value", value);
  return false;
}


bool parseValueAttribute(SDPAttributeType type, QString value,
                         QList<SDPAttribute> &valueAttributes)
{
  if(value != "")
  {
    Logger::getLogger()->printNormal("SipContent", "Correctly matched a value attribute");
    valueAttributes.push_back(SDPAttribute{type, value});
    return true;
  }

  Logger::getLogger()->printError("SipContent", "Value attribute has no value!");
  return false;
}


void parseRTPMap(QString value, QString secondWord, QList<RTPMap>& codecs)
{
  if(value != "" && secondWord != "")
  {
    QRegularExpression re_rtpParameters("(\\w+)\\/(\\w+)(\\/\\w+)?");
    QRegularExpressionMatch parameter_match = re_rtpParameters.match(secondWord);
    if(parameter_match.hasMatch() && (parameter_match.lastCapturedIndex() == 2 ||
                                      parameter_match.lastCapturedIndex() == 3))
    {
      codecs.push_back(RTPMap{static_cast<uint8_t>(value.toUInt()),
                              parameter_match.captured(2).toUInt(), parameter_match.captured(1), ""});
      if(parameter_match.lastCapturedIndex() == 3) // has codec parameters (number of audio channels)
      {
        codecs.back().codecParameter = parameter_match.captured(3);
      }
    }
    else
    {
      Logger::getLogger()->printError("SipContent", "The second part in RTPMap "
                                                    "did not match correctly for " + secondWord);
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, "SipContent", "RTPMap was not understood");
  }
}


void parseGroup(QString value, QStringList& words, QList<MediaGroup>& groups)
{
  if (value != "")
  {
    groups.push_back(MediaGroup{stringToGroupType(value), {}});

    for (int i = 1; i < words.size() ; ++i)
    {
      groups.back().identificationTags.push_back(words.at(i));
    }
  }
  else
  {
    Logger::getLogger()->printError("SIPContent", "Group has not type");
  }
}


bool parseConnection(QStringListIterator& lineIterator, char& type, QStringList& words,
                     QString& nettype, QString& addrtype, QString& address)
{
  if(type == 'c')
  {
    if(words.size() != 3)
    {
      Logger::getLogger()->printDebug(DEBUG_ERROR, "SipContent",
                                      "Wrong number of values in connection",
                                      {"values", "Expected"},
                                      {QString::number(words.size()), "3"});
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
      Logger::getLogger()->printError("SipContent", "More than one value in bitrate");
      return false;
    }

    bitrates.push_back(words.at(0));

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
      Logger::getLogger()->printError("SipContent", "More than one value in encryption key");
      return false;
    }

    key = words.at(0);
    Logger::getLogger()->printError("SipContent", "Received a encryption key field, "
                                                  "which is unsupported by us");

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
  if (words.size() < 8 || words.at(6) != "typ")
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

  if (words.size() >= 12 && words.at(8) == "raddr" && words.at(10) == "rport")
  {
    candidate->rel_address = words.at(9);
    candidate->rel_port = words.at(11).toInt();
  }

  candidates.push_back(candidate);

  return true;
}


bool parseAttributeTypeValue(QString word, SDPAttributeType& type, QString& value)
{
  QRegularExpression re_attribute("(\\w+)(?::(\\S+))?");
  QRegularExpressionMatch match = re_attribute.match(word);
  if(match.hasMatch() && match.lastCapturedIndex() >= 1)
  {
    type = stringToAttributeType(match.captured(1));

    if (match.lastCapturedIndex() == 2)
    {
      value = match.captured(2);
    }

    return true;
  }

  return false;
}


SDPAttributeType stringToAttributeType(QString attribute)
{
  // see RFC 8866
  // see RFC 5888 for mid and group

  std::map<QString, SDPAttributeType> xmap = {
         {"cat",       A_CAT},      {"keywds",    A_KEYWDS},   {"tool",      A_TOOL},
         {"maxptime",  A_MAXPTIME}, {"rtpmap",    A_RTPMAP},
         {"group",     A_GROUP},    {"mid",       A_MID},
         {"recvonly",  A_RECVONLY}, {"sendrecv",  A_SENDRECV},
         {"sendonly",  A_SENDONLY}, {"inactive",  A_INACTIVE},
         {"orient",    A_ORIENT},   {"type",      A_TYPE},     {"charset",   A_CHARSET},
         {"sdplang",   A_SDPLANG},  {"lang",      A_LANG},     {"framerate", A_FRAMERATE},
         {"quality",   A_QUALITY},  {"ptime",     A_PTIME},    {"fmtp",      A_FMTP},
         {"candidate", A_CANDIDATE}};

  if (xmap.find(attribute) == xmap.end())
  {
    Logger::getLogger()->printWarning("SIPContent", "Unrecognized attribute",
                                      "Attribute", attribute);
    return A_UNKNOWN_ATTRIBUTE;
  }

  return xmap[attribute];
}


GroupType stringToGroupType(QString group)
{
  if (group == "LS")
  {
    return G_LS;
  }
  else if (group == "FID")
  {
    return G_FID;
  }

  return G_UNRECOGNIZED;
}
