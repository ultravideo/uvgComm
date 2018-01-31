#include <sipconversions.h>

#include <QDebug>

const std::map<QString, RequestType> requestTypes = {{"INVITE", INVITE},
                                                     {"ACK", ACK},
                                                     {"BYE", BYE},
                                                     {"CANCEL", CANCEL},
                                                     {"OPTIONS", OPTIONS},
                                                     {"REGISTER", REGISTER}};

const std::map<RequestType, QString> requestStrings = {{INVITE, "INVITE"},
                                                       {ACK, "ACK"},
                                                       {BYE, "BYE"},
                                                       {CANCEL, "CANCEL"},
                                                       {OPTIONS, "OPTIONS"},
                                                       {REGISTER, "REGISTER"}};

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
                                                       {SIP_DECLINE, "DECLINE"}};

RequestType stringToRequest(QString request)
{
  if(requestTypes.find(request) == requestTypes.end())
  {
    qDebug() << "Request type not listed in conversions.";
    return SIP_UNKNOWN_REQUEST;
  }
  return requestTypes.at(request);
}

QString requestToString(RequestType request)
{
  Q_ASSERT(request != SIP_UNKNOWN_REQUEST);
  if(request == SIP_UNKNOWN_REQUEST)
  {
    return "SIP_UNKNOWN_REQUEST";
  }

  return requestStrings.at(request);
}

QString responseCodeToString(uint16_t code)
{
  return QString::number(code);
}

uint16_t stringResponseCode(QString code)
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
    qWarning() << "WARNING: Did not find response in phrase map. Maybe it has not been added yet.";
    return "PHRASE NOT ADDED";
  }

  return responsePhrases.at(response);
}
