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

  void wantAddress(QString stunServer);

signals:
  void addressReceived(QHostAddress address);

  void stunError();

private slots:
  void handleHostaddress(QHostInfo info);
  void processReply(QByteArray data);

private:

  // TODO [Encryption] Use TLS to send packet
  UDPServer udp_;

  uint8_t transactionID_[12];
};
