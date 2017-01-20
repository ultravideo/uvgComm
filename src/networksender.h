#pragma once

#include <QByteArray>
#include <QtNetwork>

// handles one connection

class NetworkSender
{
public:
  NetworkSender();

  void init(QHostAddress destination, uint16_t port);
  void sendPacket(QByteArray& data);

private:

  QUdpSocket socket_;
  QHostAddress destination_;
  uint16_t port_;

};
