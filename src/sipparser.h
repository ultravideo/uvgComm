#pragma once


#include <QHostAddress>

#include <QString>

#include "common.h"
#include <memory>


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
  QString replyAddress;
  QString branch;

  QString callID;

  uint32_t cSeq;

  QString contentType;
  QString content;
};


  // returns a null pointer if parsing is not successful
  std::unique_ptr<SIPMessageInfo> parseSIPMessage(QString& header);

