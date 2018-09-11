#pragma once

#include <QString>
#include <QList>

#include <stdint.h>

// sendrecv is default, if none present.
enum SDPAttribute {A_SENDRECV, A_SENDONLY, A_RECVONLY, A_INACTIVE};

/* SDP message info structs */

// RTP stream info
struct RTPMap
{
  uint8_t rtpNum;
  uint32_t clockFrequency;
  QString codec;
};

// SDP media info
struct MediaInfo
{
  QString type; // for example audio or video or text
  uint16_t receivePort; // rtcp is +1
  QString proto;
  QList<uint8_t> rtpNums;
  QString nettype;
  QString addrtype;
  QString address;

  QList<RTPMap> codecs;

  SDPAttribute activity;
};

// Session Description Protocol message data
struct SDPMessageInfo
{
  uint8_t version; //v=

  // o=
  QString originator_username;
  uint64_t sess_id; // set id so it does not collide
  uint64_t sess_v;  // version that is increased with each modification
  QString host_nettype;
  QString host_addrtype;
  QString host_address;

  QString sessionName; // s=
  QString sessionDescription; // i= optional

  QString email; // e= optional, not in use
  QString phone; // p= optional, not in use

  // c=, either one global or one for each media.
  QString connection_nettype;
  QString connection_addrtype;
  QString connection_address;

  // t=
  // NTP time values since 1990 ( UNIX + 2208988800 )
  // if 0 not in use.
  uint32_t startTime;
  uint32_t stopTime;

  // m=
  QList<MediaInfo> media;
};

Q_DECLARE_METATYPE(SDPMessageInfo); // used in qvariant for content
