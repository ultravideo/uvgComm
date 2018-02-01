#include <sdpparser.h>

#include <siptypes.h>

#include <QDebug>

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
          else if(additionalLine.left(8) == "a=rtpmap")
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
              qDebug() << "Detected unsupported encoding parameter in:" << additionalWords;
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
