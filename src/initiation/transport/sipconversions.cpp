#include "sipconversions.h"

#include "common.h"

#include <QDebug>


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

const std::map<SIPResponseStatus, QString> responsePhrases = {{SIP_UNKNOWN_RESPONSE, "Unknown response"},
                                                              {SIP_TRYING, "Trying"},
                                                              {SIP_RINGING, "Ringing"},
                                                              {SIP_FORWARDED, "Forwarded"},
                                                              {SIP_QUEUED, "Queued"},
                                                              {SIP_SESSION_IN_PROGRESS, "Sesssion in progress"},
                                                              {SIP_EARLY_DIALOG_TERMINATED, "Early dialog terminated"},
                                                              {SIP_OK, "Ok"},
                                                              {SIP_NO_NOTIFICATION, "No notification"},
                                                              {SIP_BAD_REQUEST, "Bad Request"},
                                                              {SIP_BUSY_HERE, "Busy"},
                                                              {SIP_DECLINE, "Call declined"}}; // TODO: finish this

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
    qDebug() << "Request type not listed in conversions.";
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
    printDebug(DEBUG_WARNING, "SIPConversions",
                     "Did not find response in phrase map. Maybe it has not been added yet.");

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
    qDebug() << "Unrecognized connection protocol:" << type;
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
    qDebug() << "WARNING: Tried to convert unrecognized protocol to string! "
             << connection << "Should be checked earlier.";
  }
  }
  return "";
}


MediaType stringToContentType(const QString &typeStr)
{
  if(mediaTypes.find(typeStr) == mediaTypes.end())
  {
    printWarning("SIPConversions",
                 "Did not find response in phrase map. Maybe it has not been added yet.");

    return MT_UNKNOWN;
  }

  return mediaTypes.at(typeStr);
}


QString contentTypeToString(const MediaType type)
{
  if(mediaStrings.find(type) == mediaStrings.end())
  {
    printWarning("SIPConversions",
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
    printProgramWarning("SIP Conversions", "Found unimplemented Auth type!");
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
    printProgramWarning("SIP Conversions", "Found unimplemented algorithm type!");
  }

  return "";
}


bool stringToBool(const QString& boolean, bool& ok)
{
  ok = true;
  if (boolean == "false")
  {
    return false;
  }
  else if (boolean == "true")
  {
    return true;
  }
  // this is neither

  printWarning("SIP Conversions", "Got neither true nor false for boolean");
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
