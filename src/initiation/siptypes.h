#pragma once

#include <QString>
#include <QList>

#include <memory>

// See RFC 3261 for INVITE, ACK, BYE, CANCEL, OPTIONS and REGISTER
// See RFC 3262 for PRACK
// See RFC 3311 for UPDATE
// See RFC 3428 for MESSAGE
// See RFC 3515 for REFER
// See RFC 3903 for PUBLISH
// See RFC 6086 for INFO
// See RFC 6665 for SUBSCRIBE and NOTIFY


enum RequestType {SIP_NO_REQUEST, SIP_INVITE, SIP_ACK, SIP_BYE, SIP_CANCEL, SIP_OPTIONS, SIP_REGISTER};
// SIP_PRACK, SIP_SUBSCRIBE, SIP_NOTIFY, SIP_PUBLISH, SIP_INFO, SIP_REFER, SIP_MESSAGE, SIP_UPDATE };

// the phrase is for humans only, so we will ignore it when parsing and use the code instead
enum ResponseType {SIP_UNKNOWN_RESPONSE = 0,
                   SIP_TRYING = 100,
                   SIP_RINGING = 180,
                   SIP_FORWARDED = 181,
                   SIP_QUEUED = 182,
                   SIP_SESSION_IN_PROGRESS = 183,
                   SIP_EARLY_DIALOG_TERMINATED = 199,
                   SIP_OK = 200,
                   SIP_ACCEPTED = 202, // depricated
                   SIP_NO_NOTIFICATION = 204, // SUBSCRIBE response. RFC 5839
                   SIP_MULTIPLE_CHOICES = 300,
                   SIP_MOVED_PERMANENTLY = 301,
                   SIP_MOVED_TEMPORARILY = 302,
                   SIP_USE_PROXY = 305,
                   SIP_ALTERNATIVE_SERVICE = 380,
                   SIP_BAD_REQUEST = 400,
                   SIP_UNAUTHORIZED = 401,
                   SIP_PAYMENT_REQUIRED = 402, // reserved
                   SIP_FORBIDDEN = 403,
                   SIP_NOT_FOUND = 404,
                   SIP_NOT_ALLOWED = 405,
                   SIP_HEADER_NOT_ACCEPTABLE = 406,
                   SIP_PROXY_AUTEHTICATION_REQUIRED = 407,
                   SIP_REQUEST_TIMEOUT = 408,
                   SIP_CONFICT = 409, // obsolete
                   SIP_GONE = 410,
                   SIP_LENGTH_REQUIRED = 411, // obsolete
                   SIP_CONDITIONAL_REQUEST_FAILED = 412,
                   SIP_REQUEST_ENTITY_TOO_LARGE = 413,
                   SIP_REQUEST_URI_TOO_LARGE = 414,
                   SIP_UNSUPPORTED_MEDIA_TYPE = 415,
                   SIP_UNSUPPORTED_URI_SCHEME = 416,
                   SIP_UNKNOWN_RESOURCE_PRIORITY = 417,
                   SIP_BAD_EXTENSION = 420,
                   SIP_EXTENSION_REQUIRED = 421,
                   SIP_SESSION_INTERVAL_TOO_SMALL = 422,
                   SIP_INTERVAL_TOO_BRIEF = 423,
                   SIP_BAD_LOCATION_INFORMATION = 424,
                   SIP_USE_IDENTIFY_HEADER = 428,
                   SIP_PROVIDE_REFERRER_IDENTITY = 429,
                   SIP_FLOW_FAILED = 430,
                   SIP_ANONYMITY_DISALLOWED = 433,
                   SIP_BAD_IDENTITY_INFO = 436,
                   SIP_UNSUPPORTED_CERTIFICATE = 437,
                   SIP_INVALID_IDENTITY_HEADER = 438,
                   SIP_FIRST_HOP_LACKS_OUTBOUND_SUPPORT = 439,
                   SIP_MAX_BREADTH_EXCEEDED = 440,
                   SIP_BAD_INFO_PACKAGE = 469,
                   SIP_CONSENT_NEEDED = 470,
                   SIP_TEMPORARILY_UNAVAILABLE = 480,
                   SIP_CALL_DOES_NOT_EXIST = 481,
                   SIP_LOOP_DETECTED = 482,
                   SIP_TOO_MANY_HOPS = 483,
                   SIP_ADDRESS_INCOMPLETE = 484,
                   SIP_AMBIGIOUS = 485,
                   SIP_BUSY_HERE = 486,
                   SIP_REQUEST_TERMINATED = 487,
                   SIP_NOT_ACCEPTABLE_HERE = 488,
                   SIP_BAD_EVENT = 489,
                   SIP_REQUEST_PENDING = 491,
                   SIP_UNDECIPHERABLE = 493,
                   SIP_SECURITY_AGREEMENT_REQUIRED = 494,
                   SIP_SERVER_ERROR = 500,
                   SIP_NOT_IMPLEMENTED = 501,
                   SIP_BAD_GATEWAY = 502,
                   SIP_SERVICE_UNAVAILABLE = 503,
                   SIP_SERVER_TIMEOUT = 504,
                   SIP_VERSION_NOT_SUPPORTED = 505,
                   SIP_MESSAGE_TOO_LARGE = 513,
                   SIP_PRECONDITION_FAILURE = 580,
                   SIP_BUSY_EVERYWHERE = 600,
                   SIP_DECLINE = 603,
                   SIP_DOES_NOT_EXIST_ANYWHERE = 604,
                   SIP_NOT_ACCEPTABLE = 606,
                   SIP_UNWANTED = 607}; // RFC 3261

enum ConnectionType {ANY, TCP, UDP, TLS};

// SIP is not secured
// SIPS is TLS secured
enum UriType {SIP, SIPS, TEL};


// 7 is the length of preset string
const uint32_t BRANCHLENGTH = 32 - 7;

struct ViaInfo
{
  ConnectionType type;
  QString version;
  QString address;
  QString branch;
};

// usually in format: "realname <sip:username@host>". realname may be empty and should be omitted if so
struct SIP_URI
{
  QString username;
  QString realname;
  QString host;
  UriType type;
};

enum ContentType {NO_CONTENT, APPLICATION_SDP, TEXT_PLAIN};

struct ContentInfo
{
  ContentType type; // tells what is in QVariant content
  uint32_t length;  // set by SIPTransport
};

/* notes on expansion of the SIP structures such as SIPDialogInfo, SIPMessageInfo, SIPRequest and SIPResponse
 * with new SIP message extensions.

 * If you want to add support for a new parameter to SIP message:
 * 1) add the parameter to desired struct,
 * 2) compose the parameter using tryAddParameter in sipfieldcomposing withing desired field and
 * 3) parse parameter using parseParameterNameToValue in sipfieldparsing

 * If you want to add support for a new field to SIP message:
 * 1) add field values to desired structs,
 * 2) add a function for composing the message to sipfieldcomposing
 * 3) use the composing function in SIPTransport
 * 4) add a function for field parsing to sipfieldparsing and
 * 5) add the parsing function to parsing map at the start of siptransport
 */

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;

// Identifies the SIP dialog
struct SIPDialogInfo
{
  QString toTag;
  QString fromTag;
  QString callID; // in form callid@host
};

// Identifies the SIP message and the transaction it belongs to as well as participants
struct SIPMessageInfo
{
  QString version;
  uint maxForwards;
  std::shared_ptr<SIPDialogInfo> dialog;
  SIP_URI from; // For dialog requests, use SIPDialog. Otherwise use SIPInfo
  SIP_URI to;

  QList<ViaInfo> senderReplyAddress;   // from via-fields. Send responses here by copying these.
  SIP_URI contact;  // Contact field. Send requests here. Mandatory in INVITE requests

  uint32_t cSeq; // must be less than 2^31
  RequestType transactionRequest;

  ContentInfo content;
};

// data in a request
struct SIPRequest
{
  RequestType type;
  SIP_URI requestURI;
  std::shared_ptr<SIPMessageInfo> message;
};

// data in a response
struct SIPResponse
{
  ResponseType type;
  std::shared_ptr<SIPMessageInfo> message;
};


// SIPParameter and SIPField are used as an intermediary step in composing and parsing SIP messages
struct SIPParameter
{
  QString name;
  QString value;
};

struct SIPField
{
  QString name;
  QString values;
  std::shared_ptr<QList<SIPParameter>> parameters;
};
