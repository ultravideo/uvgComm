#include "sipcontent.h"

#include "sip/sdptypes.h"

#include <QRegularExpression>
#include <QDebug>


bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue);
bool parseSDPMedia(QStringListIterator lineIterator, MediaInfo& media);

// called for every new line in SDP parsing. Parses out the two first characters and gives the first value,
// and the rest of the values are divided to separate words. The amount of words can be checked and
// the first character is recorded to lineType.
bool nextLine(QStringListIterator lineIterator, QStringList& words, char& lineType,
              QString& firstValue);

// Some fields simply take all the fields and put them in value regardless of spaces.
// Called after nextLine for this situation.
void gatherLine(QString& target, QString firstValue, QStringList& words);

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

bool nextLine(QStringListIterator lineIterator, QStringList& words, char& lineType,
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
      if(words.size() != 1)
      {
        // it must be an rtpmap
      }
      else if(words.size() == 1)
      {

      }
      // ignore non recognized attributes.

      QRegularExpression re_attribute("(\\w+)(:(\\S+))?");
      QRegularExpressionMatch match = re_attribute.match(firstValue);
      if(match.hasMatch())
      {
        QString attribute = "";
        QString value = "";

        const int ATTRIBUTEINDEX = 2;
        const int VALUEINDEX = 4;
        if(match.lastCapturedIndex() == ATTRIBUTEINDEX || match.lastCapturedIndex() == VALUEINDEX)
        {
          attribute = match.captured(ATTRIBUTEINDEX - 1);

          if(match.lastCapturedIndex() == VALUEINDEX)
          {
            value = match.captured(VALUEINDEX - 1);
          }

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

          }

        }

        sdp.attributes.push_back(firstValue);

      }

      // a=, m= or nothing.
      if(!nextLine(lineIterator, words, type, firstValue))
      {
        return true;
      }
    }
  }

  return true;
}

bool parseSDPMedia(QStringListIterator lineIterator, MediaInfo& media)
{

}

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
