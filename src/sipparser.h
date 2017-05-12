#pragma once


#include "common.h"

#include <QHostAddress>
#include <QString>

#include <memory>


// if strict has not been set, these can include
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

  // returns a filled SIPMessageInfo if possible, otherwise
  // returns a null pointer if parsing was not successful
  std::shared_ptr<SIPMessageInfo> parseSIPMessage(QString& header);

  std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

