#pragma once

enum RequestType {NOREQUEST, INVITE, ACK, BYE, CANCEL, OPTIONS, REGISTER}; // RFC 3261
 //PRACK,SUBSCRIBE, NOTIFY, PUBLISH, INFO, REFER, MESSAGE, UPDATE }; RFC 3262, 6665, 3903, 6086, 3515, 3428, 3311

enum ResponseType {NORESPONSE, RINGING_180, OK_200, MALFORMED_400, UNSUPPORTED_413, DECLINE_603}; // RFC 3261;

#include <QString>
#include <QList>

// All the info needed for the SIP message to find its correct recipient
struct SIPRoutingInfo
{
  //what about request vs response?

  QString senderUsername;
  QString senderRealname;
  QString senderHost;
  QList<QString> senderReplyAddress;   // from via-fields. Send responses here by copying these.
  QString contactAddress;  // from contact field. Send requests here to bypass server

  QString receiverUsername;
  QString receiverRealname;
  QString receiverHost;

  QString sessionHost;

  uint16_t maxForwards;
};

// Identifies the SIP dialog
struct SIPSessionInfo
{
  QString remoteTag;
  QString localTag;
  QString callID;
};

// Identiefies the SIP message and the transaction it belongs to
struct SIPMessage
{
  // QString version;
  QString branch;
  uint32_t cSeq; // must be less than 2^31
  RequestType transactionRequest;
};

struct SIPMessageInfo
{
  RequestType request;
  ResponseType response;
  QString version;

  QString remoteName;
  QString remoteUsername;
  QString remoteLocation;
  QString replyAddress;
  QString contactAddress;

  QString remoteTag;

  uint16_t maxForwards;

  QString localName;
  QString localUsername;
  QString localLocation;
  QString localTag;

  QString branch;

  QString callID;
  QString host;

  uint32_t cSeq;
  RequestType originalRequest;

  QString contentType;
};

// sendrecv is default, if none present.
enum SDPAttribute {A_SENDRECV, A_SENDONLY, A_RECVONLY, A_INACTIVE};

/* SDP message info structs */
struct RTPMap
{
  uint16_t rtpNum;
  uint32_t clockFrequency;
  QString codec;
};

struct MediaInfo
{
  QString type;
  uint16_t receivePort; // rtcp reports are sent to +1
  QString proto;
  uint16_t rtpNum;
  QString nettype;
  QString addrtype;
  QString address;

  QList<RTPMap> codecs;

  SDPAttribute activity;
};

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
  uint32_t startTime;
  uint32_t endTime;

  // m=
  QList<MediaInfo> media;
};
