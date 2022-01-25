#include "sipconversions.h"

#include "common.h"
#include "logger.h"


// Note: if you see a non-POD static warning, it comes from glazy. This warning
// is meant for libraries so they don't needlessly waste resources by initializing
// features which are not used.
const std::map<QString, SIPRequestMethod> requestTypes = {{"INVITE", SIP_INVITE},
                                                          {"ACK", SIP_ACK},
                                                          {"BYE", SIP_BYE},
                                                          {"CANCEL", SIP_CANCEL},
                                                          {"OPTIONS", SIP_OPTIONS},
                                                          {"REGISTER", SIP_REGISTER}};

const std::map<SIPRequestMethod, QString> requestStrings = {{SIP_INVITE, "INVITE"},
                                                            {SIP_ACK, "ACK"},
                                                            {SIP_BYE, "BYE"},
                                                            {SIP_CANCEL, "CANCEL"},
                                                            {SIP_OPTIONS, "OPTIONS"},
                                                            {SIP_REGISTER, "REGISTER"}};

const std::map<SIPResponseStatus, QString> responsePhrases = {{SIP_UNKNOWN_RESPONSE,          "Unknown response"},
                                                              {SIP_TRYING,                    "Trying"},
                                                              {SIP_RINGING,                   "Ringing"},
                                                              {SIP_FORWARDED,                 "Call is being forwarded"},
                                                              {SIP_QUEUED,                    "Queued"},
                                                              {SIP_SESSION_PROGRESS,          "Details on call progress"},
                                                              {SIP_EARLY_DIALOG_TERMINATED,   "Early dialog terminated"},
                                                              {SIP_OK,                        "Ok"},
                                                              {SIP_ACCEPTED,                  "Accepted (depricated)"},
                                                              {SIP_NO_NOTIFICATION,           "No notification"},
                                                              {SIP_MULTIPLE_CHOICES,          "Multiple choices"},
                                                              {SIP_MOVED_PERMANENTLY,         "Moved permanently"},
                                                              {SIP_MOVED_TEMPORARILY,         "Moved temporarily"},
                                                              {SIP_USE_PROXY,                 "Please use the specified proxy"},
                                                              {SIP_ALTERNATIVE_SERVICE,       "Please use the specified alternative service"},
                                                              {SIP_BAD_REQUEST,               "Request was not understood"},
                                                              {SIP_UNAUTHORIZED,              "Requires authorization"},
                                                              {SIP_PAYMENT_REQUIRED,          "Payment required (reserved)"},
                                                              {SIP_FORBIDDEN,                 "Request forbidden"},
                                                              {SIP_NOT_FOUND,                 "Not found"},
                                                              {SIP_NOT_ALLOWED,               "Not allowed"},
                                                              {SIP_NOT_ACCEPTABLE_CONTENT,    "Cannot generate response with acceptable content"},
                                                              {SIP_PROXY_AUTHENTICATION_REQUIRED, "Proxy authorization required"},
                                                              {SIP_REQUEST_TIMEOUT,           "Request has timed out"},
                                                              {SIP_CONFICT,                   "Conflict (obsolete)"},
                                                              {SIP_GONE,                      "Resource no longer here"},
                                                              {SIP_LENGTH_REQUIRED,           "Length required (obsolete)"},
                                                              {SIP_CONDITIONAL_REQUEST_FAILED, "Conditional request failed"},
                                                              {SIP_REQUEST_ENTITY_TOO_LARGE,  "Request entity too large"},
                                                              {SIP_REQUEST_URI_TOO_LARGE,     "Request-URI too long"},
                                                              {SIP_UNSUPPORTED_MEDIA_TYPE,    "Unsupported media type"},
                                                              {SIP_UNSUPPORTED_URI_SCHEME,    "Unsupported URI scheme"},
                                                              {SIP_UNKNOWN_RESOURCE_PRIORITY, "Unknown resource priority"},
                                                              {SIP_BAD_EXTENSION,             "Extension not understood"},
                                                              {SIP_EXTENSION_REQUIRED,        "Extension required"},
                                                              {SIP_SESSION_INTERVAL_TOO_SMALL, "Session-Expires field too small"},
                                                              {SIP_INTERVAL_TOO_BRIEF,        "Expires field too small"},
                                                              {SIP_BAD_LOCATION_INFORMATION,  "Location was not understood"},
                                                              {SIP_USE_IDENTIFY_HEADER,       "Please use Identify header"},
                                                              {SIP_PROVIDE_REFERRER_IDENTITY, "Please include a valid Referred-By token"},
                                                              {SIP_FLOW_FAILED,               "Flow failed"},
                                                              {SIP_ANONYMITY_DISALLOWED,      "Anonymity not allowed"},
                                                              {SIP_BAD_IDENTITY_INFO,         "Identity-Info not understood"},
                                                              {SIP_UNSUPPORTED_CERTIFICATE,   "Unable to validate domain certificate"},
                                                              {SIP_INVALID_IDENTITY_HEADER,   "Unable to verify identity signiture"},
                                                              {SIP_FIRST_HOP_LACKS_OUTBOUND_SUPPORT, "First hop lacks outbound support"},
                                                              {SIP_MAX_BREADTH_EXCEEDED,      "Max-Breadth exceeded"},
                                                              {SIP_BAD_INFO_PACKAGE,          "Bad info package"},
                                                              {SIP_CONSENT_NEEDED,            "Permission needed from recipient"},
                                                              {SIP_TEMPORARILY_UNAVAILABLE,   "Recipient temporarily unavailable"},
                                                              {SIP_CALL_DOES_NOT_EXIST,       "Request does not match dialog or transaction"},
                                                              {SIP_LOOP_DETECTED,             "Loop detected"},
                                                              {SIP_TOO_MANY_HOPS,             "Too many hops"},
                                                              {SIP_ADDRESS_INCOMPLETE,        "Request-URI incomplete"},
                                                              {SIP_AMBIGIOUS,                 "Request-URI ambigious"},
                                                              {SIP_BUSY_HERE,                 "Recipient is busy"},
                                                              {SIP_REQUEST_TERMINATED,        "Request terminated"},
                                                              {SIP_NOT_ACCEPTABLE_HERE,       "Some request aspects not acceptable"},
                                                              {SIP_BAD_EVENT,                 "Event packet not understood"},
                                                              {SIP_REQUEST_PENDING,           "Request pending"},
                                                              {SIP_UNDECIPHERABLE,            "Cannot decrypt MIME body"},
                                                              {SIP_SECURITY_AGREEMENT_REQUIRED, "Please negotiate security agreement"},
                                                              {SIP_SERVER_ERROR,              "Internal server error"},
                                                              {SIP_NOT_IMPLEMENTED,           "Not implemented"},
                                                              {SIP_BAD_GATEWAY,               "Bad gateway"},
                                                              {SIP_SERVICE_UNAVAILABLE,       "Service unavailable"},
                                                              {SIP_SERVER_TIMEOUT,            "Server timed-out"},
                                                              {SIP_VERSION_NOT_SUPPORTED,     "SIP version not supported"},
                                                              {SIP_MESSAGE_TOO_LARGE,         "Message too large"},
                                                              {SIP_PRECONDITION_FAILURE,      "Preconditions are unacceptable"},
                                                              {SIP_BUSY_EVERYWHERE,           "Busy everywhere"},
                                                              {SIP_DECLINE,                   "Call declined"},
                                                              {SIP_DOES_NOT_EXIST_ANYWHERE,   "User does not exist anywhere"},
                                                              {SIP_NOT_ACCEPTABLE,            "Session description not acceptable"},
                                                              {SIP_UNWANTED,                  "Unwanted by recipient"},
                                                              {SIP_REJECTED,                  "Rejected by intermediary process"}};

const std::map<MediaType, QString> mediaStrings = {{MT_NONE, ""},
                                                   {MT_UNKNOWN, ""},
                                                   {MT_APPLICATION, "application"},
                                                   {MT_APPLICATION_SDP, "application/sdp"},
                                                   {MT_TEXT, "text"},
                                                   {MT_AUDIO, "audio"},
                                                   {MT_AUDIO_OPUS, "audio/opus"},
                                                   {MT_VIDEO, "video"},
                                                   {MT_VIDEO_HEVC, "video/hevc"},
                                                   {MT_MESSAGE, "message"},
                                                   {MT_MULTIPART, "multipart"}};

const std::map<QString, MediaType> mediaTypes = {{"", MT_NONE},
                                                 {"application", MT_APPLICATION},
                                                 {"application/sdp", MT_APPLICATION_SDP},
                                                 {"text", MT_TEXT},
                                                 {"audio", MT_AUDIO},
                                                 {"audio/opus", MT_AUDIO_OPUS},
                                                 {"video", MT_VIDEO},
                                                 {"video/hevc", MT_VIDEO_HEVC},
                                                 {"message", MT_MESSAGE},
                                                 {"multipart", MT_MULTIPART}};




SIPRequestMethod stringToRequestMethod(const QString &request)
{
  if(requestTypes.find(request) == requestTypes.end())
  {
    Logger::getLogger()->printWarning("SIP Conversions", "Request type not listed in conversions.");
    return SIP_NO_REQUEST;
  }
  return requestTypes.at(request);
}


QString requestMethodToString(SIPRequestMethod request)
{
  Q_ASSERT(request != SIP_NO_REQUEST);
  if(request == SIP_NO_REQUEST)
  {
    return "SIP_UNKNOWN_REQUEST";
  }

  return requestStrings.at(request);
}


uint16_t stringToResponseCode(const QString &code)
{
  return code.toUInt();
}


SIPResponseStatus codeToResponseType(uint16_t code)
{
  return static_cast<SIPResponseStatus>(code);
}


uint16_t responseTypeToCode(SIPResponseStatus response)
{
  return (uint16_t)response;
}


QString codeToPhrase(uint16_t code)
{
  return responseTypeToPhrase(codeToResponseType(code));
}


QString responseTypeToPhrase(SIPResponseStatus response)
{
  if(responsePhrases.find(response) == responsePhrases.end())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "SIPConversions",
                                    "Did not find response in phrase map. "
                                    "Maybe it has not been added yet.");

    return "NO PHRASE";
  }

  return responsePhrases.at(response);
}


// connection type and string
SIPTransportProtocol stringToTransportProtocol(const QString &type)
{
  if(type == "UDP")
  {
    return UDP;
  }
  else if(type == "TCP")
  {
    return TCP;
  }
  else if(type == "TLS")
  {
    return TLS;
  }
  else
  {
    Logger::getLogger()->printWarning("SIP Conversions", "Unrecognized connection protocol.",
                 {"Protocol"}, {type});
  }
  return NONE;
}


QString transportProtocolToString(const SIPTransportProtocol connection)
{
  switch(connection)
  {
  case UDP:
  {
    return "UDP";
  }
  case TCP:
    {
      return "TCP";
    }
  case TLS:
    {
      return "TLS";
    }
  default:
  {
    Logger::getLogger()->printWarning("SIP Conversions", "Tried to convert unrecognized protocol to string! "
                                    "Should be checked earlier.",
                 {"Protocol"}, {QString::number(connection)});
  }
  }
  return "";
}


MediaType stringToContentType(const QString &typeStr)
{
  if(mediaTypes.find(typeStr) == mediaTypes.end())
  {
    Logger::getLogger()->printWarning("SIPConversions",
                 "Did not find response in phrase map. Maybe it has not been added yet.");

    return MT_UNKNOWN;
  }

  return mediaTypes.at(typeStr);
}


QString contentTypeToString(const MediaType type)
{
  if(mediaStrings.find(type) == mediaStrings.end())
  {
    Logger::getLogger()->printWarning("SIPConversions",
                 "Did not find response in phrase map. Maybe it has not been added yet.");

    return "";
  }

  return mediaStrings.at(type);
}


QopValue stringToQopValue(const QString& qop)
{
  if (qop == "")
  {
    return SIP_NO_AUTH;
  }
  if (qop == "auth")
  {
    return SIP_AUTH;
  }
  else if (qop == "auth-int")
  {
    return SIP_AUTH_INT;
  }
  // Note: add more auth types here if needed


  return SIP_AUTH_UNKNOWN;
}


QString qopValueToString(const QopValue qop)
{
  if (qop == SIP_AUTH)
  {
    return "auth";
  }
  else if (qop == SIP_AUTH_INT)
  {
    return "auth-int";
  }
  else if (qop == SIP_AUTH_UNKNOWN)
  {
    Logger::getLogger()->printProgramWarning("SIP Conversions", "Found unimplemented Auth type!");
  }
  // Note: Add more auth types here if needed

  return "";
}


DigestAlgorithm stringToAlgorithm(const QString& algorithm)
{
  if (algorithm == "")
  {
    return SIP_NO_ALGORITHM;
  }
  else if (algorithm == "MD5")
  {
    return SIP_MD5;
  }
  else if (algorithm == "MD5-sess")
  {
    return SIP_MD5_SESS;
  }

  return SIP_UNKNOWN_ALGORITHM;
}


QString algorithmToString(const DigestAlgorithm algorithm)
{
  if (algorithm == SIP_MD5)
  {
    return "MD5";
  }
  else if (algorithm == SIP_MD5_SESS)
  {
    return "MD5-sess";
  }
  else if (algorithm == SIP_UNKNOWN_ALGORITHM)
  {
    Logger::getLogger()->printProgramWarning("SIP Conversions", "Found unimplemented algorithm type!");
  }

  return "";
}


bool stringToBool(const QString& boolean, bool& ok)
{
  ok = true;
  if (boolean == "false" ||
      boolean == "FALSE" ||
      boolean == "False")
  {
    return false;
  }
  else if (boolean == "true" ||
           boolean == "TRUE" ||
           boolean == "True")
  {
    return true;
  }
  // this is neither

  Logger::getLogger()->printWarning("SIP Conversions", "Got neither true nor false for boolean");
  ok = false;
  return false;
}


QString boolToString(const bool boolean)
{
  if (boolean)
  {
    return "true";
  }

  return "false";
}


SIPPriorityField stringToPriority(const QString& priority)
{
  if (priority == "")
  {
    return SIP_NO_PRIORITY;
  }
  else if (priority == "emergency")
  {
    return SIP_EMERGENCY;
  }
  else if (priority == "urgent")
  {
    return SIP_URGENT;
  }
  else if (priority == "normal")
  {
    return SIP_NORMAL;
  }
  else if (priority == "non-urgent")
  {
    return SIP_NONURGENT;
  }

  return SIP_UNKNOWN_PRIORITY;
}


QString priorityToString(const SIPPriorityField priority)
{
  if (priority == SIP_EMERGENCY)
  {
    return "emergency";
  }
  else if (priority == SIP_URGENT)
  {
    return "urgent";
  }
  else if (priority == SIP_NORMAL)
  {
    return "normal";
  }
  else if (priority == SIP_NONURGENT)
  {
    return "non-urgent";
  }

  return "";
}




bool addParameter(QList<SIPParameter> &parameters,
                  const SIPParameter& parameter)
{
  if (parameter.name == "")
  {
    return false;
  }

  parameters.append(parameter);
  return true;
}


// this is here because it is used by both composing and parsing
void copyParameterList(const QList<SIPParameter> &inParameters,
                       QList<SIPParameter> &outParameters)
{
  for (auto& parameter : inParameters)
  {
    if (!addParameter(outParameters, parameter))
    {
      Logger::getLogger()->printProgramWarning("SIP Conversions",
                          "Failed to add parameter.");
    }
  }
}
