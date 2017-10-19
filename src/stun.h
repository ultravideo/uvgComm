#pragma once

#include "udpserver.h"

#include <QHostInfo>
#include <QHostAddress>
#include <QDnsLookup>

class Stun : public QObject
{
  Q_OBJECT
public:
  Stun();

  void wantAddress();

signals:
  void addressReceived(QHostAddress address);

private slots:
  void handleHostaddress(QHostInfo info);
  void processReply(QByteArray data);

private:
  uint32_t generate96bits(uint8_t transactionID[12]);

  UDPServer udp_;
};
