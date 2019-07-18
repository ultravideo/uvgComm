#pragma once

#include "icetypes.h"

#include <QString>
#include <QList>

#include <memory>
#include <stdint.h>


// see RFC 4566 for details.

// sendrecv is default, if none present.
// Note that RTCP is still send in case of RECVONLY, SENDONLY and INACTIVE
enum SDPAttributeType{A_CAT, A_KEYWDS, A_TOOL, A_PTIME, A_MAXPTIME, A_RTPMAP,
                      A_RECVONLY, A_SENDRECV, A_SENDONLY, A_INACTIVE,
                      A_ORIENT, A_TYPE, A_CHARSET, A_SDPLANG, A_LANG,
                      A_FRAMERATE, A_QUALITY, A_FMTP, A_CANDIDATE};

struct SDPAttribute
{
  SDPAttributeType type;
  QString value;
};

/* SDP message info structs */

// RTP stream info
struct RTPMap
{
  uint8_t rtpNum;
  uint32_t clockFrequency;
  QString codec;
  QString codecParameter; // only for audio channel count
};

// SDP media info
struct MediaInfo
{
  QString type; // for example audio, video or text
  uint16_t receivePort; // for rtp, rtcp is +1
  QString proto; // usually RTP/AVP
  QList<uint8_t> rtpNums; // stores both constant and dynamic rtp numbers

   // c=, media specific
  QString connection_nettype;
  QString connection_addrtype;
  QString connection_address;

  QString title;

  QList<QString> bitrate;       // b=, optional

  // see RFC 4567 and RFC 4568 for more details.
  QString encryptionKey;        // k=, optional

  // a=
  QList<RTPMap> codecs; // mandatory if not preset rtpnumber
  QList<SDPAttributeType> flagAttributes; // optional
  QList<SDPAttribute> valueAttributes; // optional
};

struct TimeInfo
{
  // t=
  // NTP time values since 1990 ( UNIX + 2208988800 )
  // if 0 not in use.
  uint32_t startTime;
  uint32_t stopTime;

  QString repeatInterval;
  QString activeDuration;
  QStringList offsets;
};

struct TimezoneInfo
{
  QString adjustmentTime;
  QString offset;
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

  QString sessionDescription; // i=, optional
  QString uri;                // u=, optional
  QString email;              // e=, optional
  QString phone;              // p=, optional

  // c=, global
  QString connection_nettype;
  QString connection_addrtype;
  QString connection_address;

  QList<QString> bitrate;            // b=, optional

  QList<TimeInfo> timeDescriptions; // t=, one or more

  QList<TimezoneInfo> timezoneOffsets; // z=, optional

  // see RFC 4567 and RFC 4568 for more details.
  QString encryptionKey; // k=, optional

  // a=, optional, global
  QList<SDPAttributeType> flagAttributes;
  QList<SDPAttribute> valueAttributes;

  QList<MediaInfo> media;// m=, zero or more
  QList<std::shared_ptr<ICEInfo>> candidates;
};

Q_DECLARE_METATYPE(SDPMessageInfo); // used in qvariant for content
