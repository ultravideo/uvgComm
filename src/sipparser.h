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
  QString proto;
  uint16_t rtpNum;
  QString nettype;
  QString addrtype;
  QString address;

  QList<RTPMap> codecs;
};

struct SDPMessageInfo
{
  uint8_t version;
  QString username;
  uint32_t sess_id;
  uint32_t sess_v;
  QString host_nettype;
  QString host_addrtype;
  QString hostAddress;

  QString sessionName;

  uint32_t startTime;
  uint32_t endTime;

  QString global_nettype;
  QString global_addrtype;
  QString globalAddress;

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
