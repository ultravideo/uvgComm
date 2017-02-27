#pragma once


#include <QHostAddress>

#include <QString>

#include "common.h"
#include <memory>

// TODO maybe combine this with other sip message structures such as session or compser message structure
struct SIPMessageInfo
{
  MessageType request;
  QString version;

  QString theirName;
  QString theirUsername;

  QHostAddress theirLocation;

  QString theirTag;

  uint16_t maxForwards;

  QString ourName;
  QString ourUsername;
  QString ourLocation;
  QString ourTag;
  QString replyAddress; // TODO more than one via possible
  QString branch;

  QString callID;

  uint32_t cSeq;

  QString contentType;
  QString content;
};

  // returns a null pointer if parsing is not successful
  std::unique_ptr<SIPMessageInfo> parseSIPMessage(QString& header);

