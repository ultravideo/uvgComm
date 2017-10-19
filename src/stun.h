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

  void stunError();

private slots:
  void handleHostaddress(QHostInfo info);
  void processReply(QByteArray data);

private:

  UDPServer udp_;

  uint8_t transactionID_[12];
};
