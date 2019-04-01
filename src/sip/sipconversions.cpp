#include <sip/sipconversions.h>

#include <QDebug>

const std::map<QString, RequestType> requestTypes = {{"INVITE", SIP_INVITE},
                                                     {"ACK", SIP_ACK},
                                                     {"BYE", SIP_BYE},
                                                     {"CANCEL", SIP_CANCEL},
                                                     {"OPTIONS", SIP_OPTIONS},
                                                     {"REGISTER", SIP_REGISTER}};

const std::map<RequestType, QString> requestStrings = {{SIP_INVITE, "INVITE"},
                                                       {SIP_ACK, "ACK"},
                                                       {SIP_BYE, "BYE"},
                                                       {SIP_CANCEL, "CANCEL"},
                                                       {SIP_OPTIONS, "OPTIONS"},
                                                       {SIP_REGISTER, "REGISTER"}};

const std::map<ResponseType, QString> responsePhrases = {{SIP_UNKNOWN_RESPONSE, "UNKNOWN RESPONSE"},
                                                       {SIP_TRYING, "TRYING"},
                                                       {SIP_RINGING, "RINGING"},
                                                       {SIP_FORWARDED, "SIP FORWARDED"},
                                                       {SIP_QUEUED, "SIP QUEUED"},
                                                       {SIP_SESSION_IN_PROGRESS, "SESSION IN PROGRESS"},
                                                       {SIP_EARLY_DIALOG_TERMINATED, "EARLY DIALOG TERMINATED"},
                                                       {SIP_OK, "OK"},
                                                       {SIP_NO_NOTIFICATION, "NO NOTIFICATION"},
                                                       {SIP_BAD_REQUEST, "BAD REQUEST"},
                                                       {SIP_BUSY_HERE, "BUSY_HERE"},
                                                       {SIP_DECLINE, "DECLINE"}}; // TODO: finish this

RequestType stringToRequest(QString request)
{
  if(requestTypes.find(request) == requestTypes.end())
  {
    qDebug() << "Request type not listed in conversions.";
    return SIP_NO_REQUEST;
  }
  return requestTypes.at(request);
}

QString requestToString(RequestType request)
{
  Q_ASSERT(request != SIP_NO_REQUEST);
  if(request == SIP_NO_REQUEST)
  {
    return "SIP_UNKNOWN_REQUEST";
  }

  return requestStrings.at(request);
}

uint16_t stringToResponseCode(QString code)
{
  return code.toUInt();
}

ResponseType codeToResponse(uint16_t code)
{
  return static_cast<ResponseType>(code);
}

uint16_t responseToCode(ResponseType response)
{
  return (uint16_t)response;
}

QString codeToPhrase(uint16_t code)
{
  return responseToPhrase(codeToResponse(code));
}

QString responseToPhrase(ResponseType response)
{
  if(responsePhrases.find(response) == responsePhrases.end())
  {
    qWarning() << "WARNING: Did not find response in phrase map. "
                  "Maybe it has not been added yet.";
    return "NO PHRASE";
  }

  return responsePhrases.at(response);
}

// connection type and string
ConnectionType stringToConnection(QString type)
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
  return ANY;
}

QString connectionToString(ConnectionType connection)
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

ContentType stringToContentType(QString typeStr)
{
  if(typeStr == "application/sdp")
  {
    return APPLICATION_SDP;
  }
  return NO_CONTENT;
}

QString contentTypeToString(ContentType type)
{
  switch(type)
  {
  case APPLICATION_SDP:
  {
    return "application/sdp";
  }
  default:
    break;
  }
  return "";
}
