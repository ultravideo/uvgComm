#pragma once


#include "common.h"

#include <QHostAddress>
#include <QString>

#include <memory>


// if strict has not been set, these can include
struct SIPMessageInfo
{
  MessageType request;
  QString version;

  QString theirName;
  QString theirUsername;
  QString theirLocation;
  QString replyAddress;
  QString contactAddress;

  QString theirTag;

  uint16_t maxForwards;

  QString ourName;
  QString ourUsername;
  QString ourLocation;
  QString ourTag;

  QString branch;

  QString callID;
  QString host;

  uint32_t cSeq;

  QString contentType;
};

struct RTPMap
{
  uint16_t rtpNum;
  uint32_t clockFrequency;
  QString codec;
};

struct MediaInfo
{
  QString type;
  uint16_t port;
  uint16_t rtpNum;
  QString connectionAddress;
  QList<RTPMap> codecs;
};

struct SDPMessageInfo
{
  uint8_t version;
  std::string username;
  std::string sess_id;
  std::string sess_v;
  std::string nettype;
  std::string addrtype;
  std::string hostAddress;

  std::string sessionName;

  QList<MediaInfo> media;
};

  // returns a filled SIPMessageInfo if possible, otherwise
  // returns a null pointer if parsing was not successful
  std::shared_ptr<SIPMessageInfo> parseSIPMessage(QString& header);

  std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

/*
struct SDPMessageInfo
{
  QList<uint16_t> videoPorts;
  QList<uint16_t> audioPorts;

  QList<QString> videoCodec;
  QList<QString> audioCodec;

  QString mediaLocation;
};

*/
