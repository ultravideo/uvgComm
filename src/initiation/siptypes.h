#pragma once

#include <QString>
#include <QList>

#include <memory>


/* notes on expansion of the SIP structures such as SIPMessageHeader,
 *  SIPRequest and SIPResponse with new SIP message extensions.

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


// See RFC 3261 for INVITE, ACK, BYE, CANCEL, OPTIONS and REGISTER
// See RFC 3262 for PRACK
// See RFC 3311 for UPDATE
// See RFC 3428 for MESSAGE
// See RFC 3515 for REFER
// See RFC 3903 for PUBLISH
// See RFC 6086 for INFO
// See RFC 6665 for SUBSCRIBE and NOTIFY

enum SIPRequestMethod {SIP_NO_REQUEST,
                       SIP_INVITE,
                       SIP_ACK,
                       SIP_BYE,
                       SIP_CANCEL,
                       SIP_OPTIONS,
                       SIP_REGISTER};
                       // SIP_PRACK,
                       // SIP_SUBSCRIBE,
                       // SIP_NOTIFY,
                       // SIP_PUBLISH,
                       // SIP_INFO,
                       // SIP_REFER,
                       // SIP_MESSAGE,
                       // SIP_UPDATE };

// the phrase is for humans only, so we will ignore it when parsing and use the code instead
enum SIPResponseStatus {SIP_UNKNOWN_RESPONSE = 0,
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
                        SIP_PROXY_AUTHENTICATION_REQUIRED = 407,
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





struct SIPParameter
{
  QString name;
  QString value; // optional
};


struct URIHeader
{
  QString hname;
  QString hvalue;
};


struct Hostport
{
  QString host; // hostname / IPv4address / IPv6reference
  uint16_t port = 0; // optional, omitted if 0
};

struct Userinfo
{
  QString user; // could also be telephone-subscriber
  QString password = ""; // omitted if empty
};

// is TEL part of absolute URI?
enum SIPType {SIP, SIPS, TEL};
const SIPType DEFAULT_SIP_TYPE = SIP;

// usually in format: "realname <sip:username@host>".
struct SIP_URI
{
  SIPType type;
  Userinfo userinfo; // optional

  Hostport hostport;

  QList<SIPParameter> uri_parameters;
  QList<URIHeader> headers;
};

struct AbsoluteURI
{
  QString scheme;
  QString path;
};

struct NameAddr{
  QString realname = "";// omitted if empty

  SIP_URI uri;
  //AbsoluteURI absoluteURI; this is an alternative to SIP_URI
};

// To and From field
struct ToFrom
{
  NameAddr address;
  QString tag = ""; // omitted if empty
};

const uint16_t TAG_LENGTH = 16;


enum SIPTransportProtocol {NONE, UDP, TCP, TLS, SCTP, OTHER};
const SIPTransportProtocol DEFAULT_TRANSPORT = TCP; // used for SIP transport

struct ViaField
{
  QString sipVersion;
  SIPTransportProtocol protocol;
  QString sentBy; // address this message originated from
  uint16_t port = 0;              // omitted if 0

  // parameters
  QString branch;
  bool alias = false;             // does the flag parameter exist
  bool rport = false;             // does the flag parameter exist
  uint16_t rportValue = 0;        // value parameter, omitted if 0
  QString receivedAddress = ""; // omitted if empty

  // multicast address (maddr) and related ttl go here for now
  QList<SIPParameter> otherParameters;
};

const QString MAGIC_COOKIE = "z9hG4bK";
const uint32_t BRANCH_TAIL_LENGTH = 32 - MAGIC_COOKIE.size();


enum DigestAlgorithm {SIP_NO_ALGORITHM, SIP_UNKNOWN_ALGORITHM,
                      SIP_MD5, SIP_MD5_SESS};

// support for new qop types can be added to SIP Conversions module
enum QopValue {SIP_NO_AUTH, SIP_AUTH_UNKNOWN, SIP_AUTH, SIP_AUTH_INT};

struct DigestChallenge
{
  QString realm;
  AbsoluteURI domain; // can also be abs-path
  QString nonce;
  QString opaque;
  bool stale;
  DigestAlgorithm algorithm;
  QList<QopValue> qopOptions;

  //QString authParam; refers to any further parameters
};


struct DigestResponse
{
  QString username = "";
  QString realm = ""; // proxy server where credentials are valid.
  QString nonce = "";
  std::shared_ptr<SIP_URI> digestUri = nullptr; // equal to Request-URI
  QString dresponse = ""; // 32-letter hex
  DigestAlgorithm algorithm = SIP_NO_ALGORITHM;
  QString cnonce;
  QString opaque;
  QopValue messageQop;
  QString nonceCount; // 8-letter lower hex

  //QString authParam; refers to any further parameters
};


struct SIPAuthInfo
{
  QString nextNonce;
  QopValue messageQop;

  QString responseAuth; // lower hexes
  QString cnonce; // 8-letter lower hex
  QString nonceCount;
};


enum MediaType {MT_NONE, MT_UNKNOWN,
                MT_APPLICATION, MT_APPLICATION_SDP,
                MT_TEXT,
                MT_AUDIO, MT_AUDIO_OPUS,
                MT_VIDEO, MT_VIDEO_HEVC,
                MT_MESSAGE, MT_MULTIPART};


struct SIPAccept
{
  MediaType type;
  QList<SIPParameter> parameters;
};


struct SIPAcceptGeneric
{
  QString accepted;
  QList<SIPParameter> parameters;
};


struct SIPInfo
{
  AbsoluteURI absoluteURI;
  QList<SIPParameter> parameters;
};


struct SIPRouteLocation
{
  NameAddr address;
  QList<SIPParameter> parameters;
};


struct ContentDisposition
{
  QString dispType;
  QList<SIPParameter> parameters;
};


struct CSeqField
{
  uint32_t cSeq; // must be less than 2^31
  SIPRequestMethod method;
};


struct SIPDateField
{
  QString weekday = "";
  QString date = "";
  QString time = "";
  QString timezone = "GMT";
};

struct SIPRetryAfter
{
  uint32_t time;
  uint32_t duration = 0;
  QList<SIPParameter> parameters;
};

enum SIPWarningCode {SIP_INCOMPATIBLE_NET_PROTOCOL = 300,
                    SIP_INCOMPATIBLE_ADDRESS_FORMAT = 301,
                    SIP_INCOMPATIBLE_TRANSPORT_PROTOCOL = 302,
                    SIP_INCOMPATIBLE_BANDWIDTH_UNIT = 303,
                    SIP_MEDIA_TYPE_NOT_AVAILABLE = 304,
                    SIP_INCOMPATIBLE_MEDIA_FORMAT = 305,
                    SIP_ATTRIBUTE_NOT_UNDERSTOOD = 306,
                    SIP_SESS_DESC_PARAMETER_NOT_UNDERSTOOD = 307,
                    SIP_MULTICAST_NOT_AVAILABLE = 330,
                    SIP_UNICAST_NOT_AVAILABLE = 331,
                    SIP_INSUFFICIENT_BANDWIDTH = 370,
                    SIP_MISCELLANEOUS_WARNING = 399};

struct SIPWarningField
{
  SIPWarningCode code;

  // hostport or pseudonym. Used for debugging
  QString warnAgent;
  QString warnText;
};



enum SIPPriorityField {SIP_NO_PRIORITY,
                       SIP_UNKNOWN_PRIORITY,
                       SIP_EMERGENCY,
                       SIP_URGENT,
                       SIP_NORMAL,
                       SIP_NONURGENT};


// - Mandatory: the processing will fail if the field is not included.
// - Should: field should be there, but client is able to receive the
//   message without that field.

// If pointer is null, the field does/should not exist in message
// This also applies to some of the tables, but not all. Those it
// does not apply are in a pointer.
struct SIPMessageHeader
{
  // callID, cSeq, from, to and via fields are copied from request to response.
  // Max-Forwards is mandatory in requests and not allowed in responses

  // INVITE and INVITE 2xx response must include contact and supported field

  // More details on which fields belong in which message and their purpose are
  // listed in RFC 3261 section 20. Details of field BNF forms are given in section 25.

  // Each variable represents one field.  Unless it has the comment mandatory,
  // the default value is set so that field does not exist.

  // All fields which allow comma separation, multiple fields with same name can
  // be treated as being comma separated lists

  // accepted content types
  std::shared_ptr<QList<SIPAccept>>        accept = nullptr;

  // accepted content encodings
  std::shared_ptr<QList<SIPAcceptGeneric>> acceptEncoding = nullptr;

  // accepted languages in SIP phrases
  std::shared_ptr<QList<SIPAcceptGeneric>> acceptLanguage = nullptr;

  // server specific ring-tone
  QList<SIPInfo>                           alertInfos = {};

  // allowed requests
  std::shared_ptr<QList<SIPRequestMethod>> allow = nullptr;

  // info about successful authentication
  std::shared_ptr<SIPAuthInfo>             authInfo  = {};

  // credentials of UA
  std::shared_ptr<QList<DigestResponse>>   authorization = nullptr;

  // mandatory, identify dialog along with tags
  QString                                  callID = "";

  // additional info from caller or callee
  QList<SIPInfo>                           callInfos = {};

  // address where to send further requests
  QList<SIPRouteLocation>                  contact = {};

  // the purpose of content
  std::shared_ptr<ContentDisposition>      contentDisposition = nullptr;

  // how the content is encoded
  QStringList                              contentEncoding = {};

  // language of content
  QStringList                              contentLanguage = {};

  // mandatory, size of the content in bytes
  uint64_t                                 contentLength = 0;

  // mandatory if content, content media type
  MediaType                                contentType = MT_NONE;

  // mandatory, identifies transactions within dialog
  CSeqField                                cSeq;

  // practically useless, gives date to end system
  std::shared_ptr<SIPDateField>            date = nullptr;

  // additional info about error
  QList<SIPInfo>                           errorInfos = {};

  // expiration time of message or content in seconds
  std::shared_ptr<uint32_t>                expires = nullptr;

  // mandatory, who started this transaction
  ToFrom                                   from;

  // if you want to return a call later
  QString                                  inReplyToCallID = "";

  // avoids loops in routing
  std::shared_ptr<uint8_t>                 maxForwards = nullptr;

  // you should at least this expires value
  std::shared_ptr<uint32_t>                minExpires = nullptr;

  // useless, always use 1.0
  QString                                  mimeVersion = "";

  // tells the organization
  QString                                  organization = "";

  // request priority to receiving user
  SIPPriorityField                         priority = SIP_NO_PRIORITY;

  // proxy authentication challenge
  std::shared_ptr<QList<DigestChallenge>>  proxyAuthenticate = nullptr;

  // proxy authentication credentials
  std::shared_ptr<QList<DigestResponse>>   proxyAuthorization = nullptr;

  // proxy must support these features in order to process request
  QStringList                              proxyRequires = {};

  // proxy wants to be included in the routing
  QList<SIPRouteLocation>                  recordRoutes = {};

  // a separate return URI or call me here URI
  std::shared_ptr<SIPRouteLocation>        replyTo = nullptr;

  // options that UAS must support
  QStringList                              require= {};

  // how long until the service is available again
  std::shared_ptr<SIPRetryAfter>           retryAfter = nullptr;

  // forces routing of request and is based on received record-routes
  QList<SIPRouteLocation>                  routes = {};

  // name of server software
  QString                                  server = "";

  // summary or nature of the call and can be used to filter calls
  QString                                  subject = "";

  // list of supported extensions
  std::shared_ptr<QStringList>             supported = nullptr;

  // request sending timestamp and can be used to calculate RTD
  QString                                  timestamp = "";

  // mandatory, logical recipient of the request
  ToFrom                                   to;

  // lists features not supported by UAS
  QStringList                              unsupported = {};

  // describes the software or Kvazzup in this case
  QString                                  userAgent = "";

  // mandatory, path taken by request so far and return path for responses
  QList<ViaField>                          vias = {};

  // additional info on response status
  QList<SIPWarningField>                   warning = {};

  // authentication challenge
  std::shared_ptr<QList<DigestChallenge>>  wwwAuthenticate = nullptr;
};

// 70 is recommended by specification
// The purpose of max-forwards is to avoid infinite routing loops.
const uint8_t DEFAULT_MAX_FORWARDS = 70;

const uint16_t CALLIDLENGTH = 16;

struct SIPRequest
{
  // currently we ignore recording the human readable phrase from incoming messages
  SIPRequestMethod method;
  // There is also something called absoluteURI which is not supported at the moment
  SIP_URI requestURI;
  QString sipVersion;

  std::shared_ptr<SIPMessageHeader> message;
};


struct SIPResponse
{
  QString sipVersion;
  SIPResponseStatus type;
  QString text;
  std::shared_ptr<SIPMessageHeader> message;
};

const QString SIP_VERSION = "2.0";

// these structs are used as a temporary step in parsing and composing SIP messages

// One set of values for a SIP field. Separated by commas
// or as separate header fields with same name.
struct SIPCommaValue
{
  QStringList words;
  QList<SIPParameter> parameters;
};

// fieldname: commaValue, commaValue
struct SIPField
{
  QString name; // before ':'
  QList<SIPCommaValue> commaSeparated; // separated by comma(,)
};
