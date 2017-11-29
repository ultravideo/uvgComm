#pragma once


#include "siptypes.h"

#include <QHostAddress>
#include <QString>

#include <memory>


  // returns a filled SIPMessageInfo if possible, otherwise
  // returns a null pointer if parsing was not successful
  std::shared_ptr<SIPMessageInfo> parseSIPMessage(QString& header);

  std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

  QList<QHostAddress> parseIPAddress(QString address);
